/*
 * Copyright (c) 2026 Hardik Bhaskar
 * Licensed under the MIT License.
 *
 * wm.rs — Hybrid BSP Tiling / Floating Window Manager for MahinaOS.
 *
 * Architecture:
 *   - BSP tree per workspace: new windows split the focused node
 *   - Floating mode per window: toggled with Super+V
 *   - 6 dynamic workspaces
 *   - Window positions communicated back to compositor via WmSetSurfacePosition
 */

use std::collections::HashMap;
use lgp::connection::LgpConnection;
use lgp::protocol::*;
use lgp::wm::WmState as LgpWm;

// ── Constants ────────────────────────────────────────────────────────────────

pub const NUM_WORKSPACES: usize = 6;
const TASKBAR_H: u32 = 32;
const DOCK_H: u32 = 64 + 8; // dock + bottom margin
const TILING_GAP: i32 = 6;
const BORDER_W: i32 = 2;

// ── Data types ────────────────────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum WmWindowState {
    Normal,
    Maximized,
    Fullscreen,
    Minimized,
    Hidden,
}

#[derive(Debug, Clone)]
pub struct WindowInfo {
    pub surface_id: u32,
    pub workspace: usize,
    pub floating: bool,
    pub x: i32,
    pub y: i32,
    pub w: u32,
    pub h: u32,
    pub state: WmWindowState,
    pub title: String,
    // For floating windows: saved tiling rect so we can restore on un-float
    pub saved_tiled_x: i32,
    pub saved_tiled_y: i32,
    pub saved_tiled_w: u32,
    pub saved_tiled_h: u32,
}

#[derive(Debug)]
enum BspNode {
    Leaf(u32),  // surface_id
    Split {
        dir: SplitDir,
        ratio: f32,     // 0.0..1.0 — left/top fraction
        left: Box<BspNode>,
        right: Box<BspNode>,
    },
}

#[derive(Debug, Clone, Copy)]
enum SplitDir {
    Horizontal, // Left | Right
    Vertical,   // Top  | Bottom
}

struct Workspace {
    root: Option<BspNode>,
    focused: Option<u32>,
}

impl Workspace {
    fn new() -> Self { Self { root: None, focused: None } }

    fn insert(&mut self, surface_id: u32) {
        let focused = self.focused;
        let new_node = BspNode::Leaf(surface_id);
        match self.root.take() {
            None => {
                self.root = Some(new_node);
            }
            Some(root) => {
                self.root = Some(insert_into_bsp(root, focused, new_node));
            }
        }
        self.focused = Some(surface_id);
    }

    fn remove(&mut self, surface_id: u32) {
        if let Some(root) = self.root.take() {
            self.root = remove_from_bsp(root, surface_id);
        }
        if self.focused == Some(surface_id) {
            self.focused = self.root.as_ref().and_then(|r| first_leaf(r));
        }
    }

    fn leaves(&self) -> Vec<u32> {
        match &self.root {
            None => vec![],
            Some(r) => collect_leaves(r),
        }
    }

    fn next_after(&self, current: u32) -> Option<u32> {
        let leaves = self.leaves();
        if leaves.is_empty() { return None; }
        let pos = leaves.iter().position(|&s| s == current).unwrap_or(0);
        let next = (pos + 1) % leaves.len();
        Some(leaves[next])
    }

    fn prev_before(&self, current: u32) -> Option<u32> {
        let leaves = self.leaves();
        if leaves.is_empty() { return None; }
        let pos = leaves.iter().position(|&s| s == current).unwrap_or(0);
        let prev = if pos == 0 { leaves.len() - 1 } else { pos - 1 };
        Some(leaves[prev])
    }
}

// ── BSP helpers ───────────────────────────────────────────────────────────────

fn first_leaf(node: &BspNode) -> Option<u32> {
    match node {
        BspNode::Leaf(id) => Some(*id),
        BspNode::Split { left, .. } => first_leaf(left),
    }
}

fn collect_leaves(node: &BspNode) -> Vec<u32> {
    match node {
        BspNode::Leaf(id) => vec![*id],
        BspNode::Split { left, right, .. } => {
            let mut l = collect_leaves(left);
            l.extend(collect_leaves(right));
            l
        }
    }
}

/// Insert a new leaf next to the focused leaf, splitting it.
fn insert_into_bsp(node: BspNode, focused: Option<u32>, new_node: BspNode) -> BspNode {
    match node {
        BspNode::Leaf(id) => {
            // Split this leaf if it's the focused one (or any if no focus)
            let dir = if focused == Some(id) || focused.is_none() {
                // Alternate H/V based on some heuristic — use leaf id parity
                if id % 2 == 0 { SplitDir::Horizontal } else { SplitDir::Vertical }
            } else {
                return BspNode::Leaf(id); // Not the target, shouldn't happen for non-focused
            };
            BspNode::Split {
                dir,
                ratio: 0.5,
                left: Box::new(BspNode::Leaf(id)),
                right: Box::new(new_node),
            }
        }
        BspNode::Split { dir, ratio, left, right } => {
            // Find which branch contains the focused leaf and insert there
            let left_leaves = collect_leaves(&left);
            if focused.map_or(true, |f| left_leaves.contains(&f)) {
                BspNode::Split {
                    dir, ratio,
                    left: Box::new(insert_into_bsp(*left, focused, new_node)),
                    right,
                }
            } else {
                BspNode::Split {
                    dir, ratio,
                    left,
                    right: Box::new(insert_into_bsp(*right, focused, new_node)),
                }
            }
        }
    }
}

/// Remove a leaf from the BSP tree. If the leaf's parent has only that leaf left,
/// the sibling promotes up.
fn remove_from_bsp(node: BspNode, target: u32) -> Option<BspNode> {
    match node {
        BspNode::Leaf(id) => {
            if id == target { None } else { Some(BspNode::Leaf(id)) }
        }
        BspNode::Split { dir, ratio, left, right } => {
            let new_left = remove_from_bsp(*left, target);
            let new_right = remove_from_bsp(*right, target);
            match (new_left, new_right) {
                (None, None) => None,
                (Some(l), None) => Some(l),
                (None, Some(r)) => Some(r),
                (Some(l), Some(r)) => Some(BspNode::Split {
                    dir, ratio,
                    left: Box::new(l),
                    right: Box::new(r),
                }),
            }
        }
    }
}

/// Compute pixel rects for every leaf, given a bounding rect.
fn layout_bsp(node: &BspNode, x: i32, y: i32, w: i32, h: i32,
              out: &mut HashMap<u32, (i32, i32, u32, u32)>) {
    if w <= 0 || h <= 0 { return; }
    match node {
        BspNode::Leaf(id) => {
            let gx = x + TILING_GAP;
            let gy = y + TILING_GAP;
            let gw = (w - TILING_GAP * 2).max(1) as u32;
            let gh = (h - TILING_GAP * 2).max(1) as u32;
            out.insert(*id, (gx, gy, gw, gh));
        }
        BspNode::Split { dir, ratio, left, right } => {
            match dir {
                SplitDir::Horizontal => {
                    let lw = ((w as f32 * ratio) as i32).clamp(1, w - 1);
                    layout_bsp(left,  x,      y, lw,     h, out);
                    layout_bsp(right, x + lw, y, w - lw, h, out);
                }
                SplitDir::Vertical => {
                    let lh = ((h as f32 * ratio) as i32).clamp(1, h - 1);
                    layout_bsp(left,  x, y,      w, lh,     out);
                    layout_bsp(right, x, y + lh, w, h - lh, out);
                }
            }
        }
    }
}

/// Adjust the split ratio adjacent to the focused leaf.
fn adjust_ratio(node: &mut BspNode, target: u32, delta: f32) {
    if let BspNode::Split { left, right, ratio, .. } = node {
        let left_leaves = collect_leaves(left);
        let right_leaves = collect_leaves(right);
        if left_leaves.contains(&target) || right_leaves.contains(&target) {
            *ratio = (*ratio + delta).clamp(0.15, 0.85);
        }
        adjust_ratio(left, target, delta);
        adjust_ratio(right, target, delta);
    }
}

// ── WmManager ─────────────────────────────────────────────────────────────────

pub struct WmManager {
    workspaces: Vec<Workspace>,
    pub active_workspace: usize,
    pub windows: HashMap<u32, WindowInfo>,
    screen_w: u32,
    screen_h: u32,
    pub focused_surface: Option<u32>,
    drag_offset: Option<(i32, i32)>, // (mouse_dx, mouse_dy) from window origin
    drag_surface: Option<u32>,
}

impl WmManager {
    pub fn new(screen_w: u32, screen_h: u32) -> Self {
        let mut workspaces = Vec::with_capacity(NUM_WORKSPACES);
        for _ in 0..NUM_WORKSPACES { workspaces.push(Workspace::new()); }
        Self {
            workspaces,
            active_workspace: 0,
            windows: HashMap::new(),
            screen_w, screen_h,
            focused_surface: None,
            drag_offset: None,
            drag_surface: None,
        }
    }

    pub fn workspace_count(&self) -> usize { NUM_WORKSPACES }

    pub fn on_surface_created(&mut self, surface_id: u32, type_: u32, width: u32, height: u32, conn: &mut LgpConnection) {
        let fw = width.min(self.screen_w);
        let fh = height.min(self.screen_h - TASKBAR_H - DOCK_H);
        let x = ((self.screen_w - fw) / 2) as i32;
        let y = TASKBAR_H as i32 + ((self.screen_h - TASKBAR_H - DOCK_H - fh) / 2) as i32;

        let win = WindowInfo {
            surface_id,
            workspace: self.active_workspace,
            floating: true,
            x, y,
            w: fw,
            h: fh,
            state: WmWindowState::Normal,
            title: format!("Window {}", surface_id),
            saved_tiled_x: 0, saved_tiled_y: 0,
            saved_tiled_w: self.screen_w,
            saved_tiled_h: self.screen_h - TASKBAR_H - DOCK_H,
        };
        self.windows.insert(surface_id, win);
        self.workspaces[self.active_workspace].insert(surface_id);
        self.focused_surface = Some(surface_id);
        self.apply_layout(conn);
        let _ = LgpWm::set_focus(conn, surface_id);
    }

    /// Called when compositor reports a surface was destroyed.
    pub fn on_surface_destroyed(&mut self, surface_id: u32, conn: &mut LgpConnection) {
        if let Some(win) = self.windows.remove(&surface_id) {
            self.workspaces[win.workspace].remove(surface_id);
        }
        if self.focused_surface == Some(surface_id) {
            let new_focus = self.workspaces[self.active_workspace]
                .leaves().into_iter().next();
            self.focused_surface = new_focus;
            if let Some(f) = new_focus {
                let _ = LgpWm::set_focus(conn, f);
            }
        }
        self.apply_layout(conn);
    }

    /// Switch to a different workspace, hiding windows not on active ws.
    pub fn switch_workspace(&mut self, idx: usize, conn: &mut LgpConnection) {
        if idx >= NUM_WORKSPACES || idx == self.active_workspace { return; }

        // Hide windows on old workspace and restore windows on new workspace
        let old_ws = self.active_workspace;
        for (&sid, win) in self.windows.iter() {
            if win.workspace == old_ws {
                let _ = LgpWm::set_state(conn, sid, LGP_WM_STATE_HIDDEN);
            } else if win.workspace == idx {
                let _ = LgpWm::set_state(conn, sid, LGP_WM_STATE_NORMAL);
            }
        }

        self.active_workspace = idx;

        // Set focus to the first window on new workspace
        self.focused_surface = self.workspaces[idx].focused;
        if let Some(f) = self.focused_surface {
            let _ = LgpWm::set_focus(conn, f);
        }

        self.apply_layout(conn);
    }

    /// Move focused window to another workspace.
    pub fn move_focused_to_workspace(&mut self, idx: usize, conn: &mut LgpConnection) {
        let sid = match self.focused_surface { Some(s) => s, None => return };
        if idx >= NUM_WORKSPACES { return; }

        let old_ws = if let Some(w) = self.windows.get(&sid) { w.workspace } else { return };
        if old_ws == idx { return; }

        // Remove from old workspace BSP
        self.workspaces[old_ws].remove(sid);
        // Update window's workspace
        if let Some(w) = self.windows.get_mut(&sid) { w.workspace = idx; }
        // Insert into new workspace BSP
        self.workspaces[idx].insert(sid);
        // Hide it since we're not switching
        let _ = LgpWm::set_state(conn, sid, LGP_WM_STATE_HIDDEN);
        self.focused_surface = self.workspaces[self.active_workspace].leaves().into_iter().next();
        self.apply_layout(conn);
    }

    /// Toggle floating mode for focused window.
    pub fn toggle_floating(&mut self, conn: &mut LgpConnection) {
        let sid = match self.focused_surface { Some(s) => s, None => return };
        if let Some(win) = self.windows.get_mut(&sid) {
            win.floating = !win.floating;
            if win.floating {
                // Save tiled position, move to center
                win.saved_tiled_x = win.x; win.saved_tiled_y = win.y;
                win.saved_tiled_w = win.w; win.saved_tiled_h = win.h;
                let fw = self.screen_w * 2 / 3;
                let fh = (self.screen_h - TASKBAR_H - DOCK_H) * 2 / 3;
                win.x = ((self.screen_w - fw) / 2) as i32;
                win.y = TASKBAR_H as i32 + ((self.screen_h - TASKBAR_H - DOCK_H - fh) / 2) as i32;
                win.w = fw; win.h = fh;
            } else {
                // Restore tiled position
                win.x = win.saved_tiled_x; win.y = win.saved_tiled_y;
                win.w = win.saved_tiled_w; win.h = win.saved_tiled_h;
            }
        }
        self.apply_layout(conn);
    }

    /// Toggle fullscreen for focused window.
    pub fn toggle_fullscreen(&mut self, conn: &mut LgpConnection) {
        let sid = match self.focused_surface { Some(s) => s, None => return };
        if let Some(win) = self.windows.get_mut(&sid) {
            match win.state {
                WmWindowState::Fullscreen => {
                    win.state = WmWindowState::Normal;
                    let _ = LgpWm::set_state(conn, sid, LGP_WM_STATE_NORMAL);
                }
                _ => {
                    win.state = WmWindowState::Fullscreen;
                    win.x = 0; win.y = 0;
                    win.w = self.screen_w; win.h = self.screen_h;
                    let _ = LgpWm::set_state(conn, sid, LGP_WM_STATE_FULLSCREEN);
                    let _ = win.set_position(conn);
                    return;
                }
            }
        }
        self.apply_layout(conn);
    }

    /// Focus the next window in the current workspace.
    pub fn focus_next(&mut self, conn: &mut LgpConnection) {
        let ws = &self.workspaces[self.active_workspace];
        let next = self.focused_surface
            .and_then(|f| ws.next_after(f))
            .or_else(|| ws.leaves().into_iter().next());
        if let Some(f) = next {
            self.focused_surface = Some(f);
            self.workspaces[self.active_workspace].focused = Some(f);
            let _ = LgpWm::set_focus(conn, f);
        }
    }

    /// Focus the previous window.
    pub fn focus_prev(&mut self, conn: &mut LgpConnection) {
        let ws = &self.workspaces[self.active_workspace];
        let prev = self.focused_surface
            .and_then(|f| ws.prev_before(f))
            .or_else(|| ws.leaves().into_iter().last());
        if let Some(f) = prev {
            self.focused_surface = Some(f);
            self.workspaces[self.active_workspace].focused = Some(f);
            let _ = LgpWm::set_focus(conn, f);
        }
    }

    /// Set focus to a specific surface (e.g., from mouse click).
    pub fn focus_surface(&mut self, surface_id: u32, conn: &mut LgpConnection) {
        if self.windows.contains_key(&surface_id) {
            self.focused_surface = Some(surface_id);
            self.workspaces[self.active_workspace].focused = Some(surface_id);
            let _ = LgpWm::set_focus(conn, surface_id);
        }
    }

    /// Snap focused window to an edge/corner.
    pub fn snap(&mut self, direction: SnapDir, conn: &mut LgpConnection) {
        let sid = match self.focused_surface { Some(s) => s, None => return };
        let work_y = TASKBAR_H as i32;
        let work_h = (self.screen_h - TASKBAR_H - DOCK_H) as i32;
        let work_w = self.screen_w as i32;
        let half_w = work_w / 2;
        let half_h = work_h / 2;

        if let Some(win) = self.windows.get_mut(&sid) {
            win.floating = true;
            match direction {
                SnapDir::Left  => { win.x=0;      win.y=work_y; win.w=half_w as u32; win.h=work_h as u32; }
                SnapDir::Right => { win.x=half_w; win.y=work_y; win.w=half_w as u32; win.h=work_h as u32; }
                SnapDir::Up    => { win.x=0;      win.y=work_y; win.w=work_w as u32; win.h=work_h as u32; }
                SnapDir::Down  => { // Restore/center
                    let fw = (work_w * 2 / 3) as u32;
                    let fh = (work_h * 2 / 3) as u32;
                    win.x = (work_w as u32 - fw) as i32 / 2;
                    win.y = work_y + (work_h as u32 - fh) as i32 / 2;
                    win.w = fw; win.h = fh;
                }
            }
        }
        if let Some(win) = self.windows.get(&sid) {
            let _ = win.set_position(conn);
        }
    }

    pub fn resize_split(&mut self, grow: bool, conn: &mut LgpConnection) {
        let sid = match self.focused_surface { Some(s) => s, None => return };
        let delta = if grow { 0.05 } else { -0.05 };
        let ws = &mut self.workspaces[self.active_workspace];
        if let Some(ref mut root) = ws.root {
            adjust_ratio(root, sid, delta);
        }
        self.apply_layout(conn);
    }

    /// Re-tile all non-floating windows on the current workspace.
    pub fn apply_layout(&mut self, conn: &mut LgpConnection) {
        let ws_idx = self.active_workspace;
        let ws = &self.workspaces[ws_idx];

        // Compute tiling area
        let tile_x = 0i32;
        let tile_y = TASKBAR_H as i32;
        let tile_w = self.screen_w as i32;
        let tile_h = (self.screen_h - TASKBAR_H - DOCK_H) as i32;

        // Get layout rects from BSP
        let mut rects: HashMap<u32, (i32, i32, u32, u32)> = HashMap::new();
        if let Some(ref root) = ws.root {
            layout_bsp(root, tile_x, tile_y, tile_w, tile_h, &mut rects);
        }

        // Apply positions: tiled windows use BSP rect, floating use their own
        for (&sid, win) in self.windows.iter_mut() {
            if win.workspace != ws_idx { continue; }
            if win.state == WmWindowState::Hidden || win.state == WmWindowState::Minimized { continue; }
            if win.state == WmWindowState::Fullscreen { continue; } // already set

            if !win.floating {
                if let Some(&(rx, ry, rw, rh)) = rects.get(&sid) {
                    win.x = rx; win.y = ry; win.w = rw; win.h = rh;
                    win.saved_tiled_x = rx; win.saved_tiled_y = ry;
                    win.saved_tiled_w = rw; win.saved_tiled_h = rh;
                }
            }
            let _ = win.set_position(conn);
        }
    }

    /// Called when pointer moves — handle drag for floating windows.
    pub fn on_pointer_motion(&mut self, x: f64, y: f64, conn: &mut LgpConnection) {
        if let (Some(sid), Some((dx, dy))) = (self.drag_surface, self.drag_offset) {
            if let Some(win) = self.windows.get_mut(&sid) {
                if win.floating {
                    win.x = x as i32 - dx;
                    win.y = y as i32 - dy;
                    let _ = win.set_position(conn);
                }
            }
        }
    }

    /// Handle a click — start drag if on a floating window title area.
    pub fn on_pointer_button(&mut self, x: f64, y: f64, _button: u32, pressed: bool, conn: &mut LgpConnection) {
        if !pressed {
            self.drag_surface = None;
            self.drag_offset = None;
            return;
        }

        // Find topmost floating window under cursor (checking titlebar area)
        let mut hit = None;
        for (&sid, win) in &self.windows {
            if win.workspace != self.active_workspace { continue; }
            if !win.floating { continue; }
            let (wx, wy, ww) = (win.x, win.y, win.w as i32);
            let xi = x as i32;
            let yi = y as i32;
            if xi >= wx && xi < wx + ww && yi >= wy - 24 && yi < wy {
                hit = Some((sid, xi - wx, yi - (wy - 24)));
            }
        }

        if let Some((sid, dx, dy)) = hit {
            self.drag_surface = Some(sid);
            self.drag_offset = Some((dx, dy));
            self.focus_surface(sid, conn);
        }
    }

    /// Get list of windows on the active workspace for taskbar rendering.
    pub fn active_windows(&self) -> Vec<&WindowInfo> {
        self.windows.values()
            .filter(|w| w.workspace == self.active_workspace
                && w.state != WmWindowState::Minimized)
            .collect()
    }
}

pub enum SnapDir { Left, Right, Up, Down }

// Helper: send position update to compositor
impl WindowInfo {
    fn set_position(&self, conn: &mut LgpConnection) -> std::io::Result<()> {
        let mut payload = vec![0u8; 12];
        payload[0..4].copy_from_slice(&self.surface_id.to_le_bytes());
        payload[4..8].copy_from_slice(&self.x.to_le_bytes());
        payload[8..12].copy_from_slice(&self.y.to_le_bytes());
        let msg = lgp::protocol::LgpMessage::new(
            LgpMessageType::WmSetSurfacePosition as u16, &payload);
        msg.send_to(&mut conn.stream)
    }
}
