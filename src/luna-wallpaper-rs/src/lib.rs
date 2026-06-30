/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

use std::ffi::c_void;
use std::fs;
use std::path::Path;
use std::ptr;
use std::slice;
use std::time::UNIX_EPOCH;

const VIDEO_FILE_PATH: &str = "/usr/share/wallpaper/live.lraw";
const LRAW_HEADER_SIZE: usize = 16;
const MAX_STARS: usize = 140;

#[repr(C)]
pub enum WallpaperTod {
    Morning = 0,
    Afternoon = 1,
    Night = 2,
}

#[repr(C)]
pub struct WallpaperState {
    engine: *mut WallpaperEngine,
}

#[derive(Clone, Copy)]
struct Star {
    x: u16,
    y: u16,
    brightness: u8,
    twinkle: u8,
}

struct VideoLoop {
    data: Vec<u8>,
    width: u32,
    height: u32,
    frames: u32,
    modified_secs: u64,
    byte_len: u64,
}

struct WallpaperEngine {
    tick: u32,
    width: u32,
    height: u32,
    tod: WallpaperTod,
    stars: Vec<Star>,
    video: Option<VideoLoop>,
}

fn lcg_next(state: &mut u32) -> u32 {
    *state = state.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
    *state
}

fn read_u32_le(bytes: &[u8]) -> Option<u32> {
    if bytes.len() < 4 {
        return None;
    }
    Some(u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
}

fn modified_secs(meta: &fs::Metadata) -> u64 {
    meta.modified()
        .ok()
        .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

fn load_video_loop(path: &Path) -> Option<VideoLoop> {
    let meta = fs::metadata(path).ok()?;
    let byte_len = meta.len();
    if byte_len < LRAW_HEADER_SIZE as u64 {
        return None;
    }

    let data = fs::read(path).ok()?;
    if data.len() < LRAW_HEADER_SIZE || &data[0..4] != b"LRAW" {
        return None;
    }

    let width = read_u32_le(&data[4..8])?;
    let height = read_u32_le(&data[8..12])?;
    let frames = read_u32_le(&data[12..16])?;
    if width == 0 || height == 0 || frames == 0 {
        return None;
    }

    let frame_bytes = width.checked_mul(height)?.checked_mul(4)? as usize;
    let expected = LRAW_HEADER_SIZE.checked_add(frame_bytes.checked_mul(frames as usize)?)?;
    if data.len() < expected {
        return None;
    }

    Some(VideoLoop {
        data,
        width,
        height,
        frames,
        modified_secs: modified_secs(&meta),
        byte_len,
    })
}

fn current_tod() -> WallpaperTod {
    let uptime = fs::read_to_string("/proc/uptime")
        .ok()
        .and_then(|s| s.split_whitespace().next().map(str::to_owned))
        .and_then(|s| s.parse::<f64>().ok())
        .unwrap_or(0.0);

    let phase = (uptime as u64) % 180;
    if phase < 60 {
        WallpaperTod::Night
    } else if phase < 120 {
        WallpaperTod::Morning
    } else {
        WallpaperTod::Afternoon
    }
}

fn blend_colour(a: u32, b: u32, t: u32) -> u32 {
    let ar = (a >> 16) & 0xff;
    let ag = (a >> 8) & 0xff;
    let ab = a & 0xff;
    let br = (b >> 16) & 0xff;
    let bg = (b >> 8) & 0xff;
    let bb = b & 0xff;
    let r = (ar * (255 - t) + br * t) / 255;
    let g = (ag * (255 - t) + bg * t) / 255;
    let bl = (ab * (255 - t) + bb * t) / 255;
    (r << 16) | (g << 8) | bl
}

fn generate_stars(width: u32, height: u32) -> Vec<Star> {
    let mut seed = width ^ (height << 16) ^ 0x4d41_4849;
    let mut stars = Vec::with_capacity(MAX_STARS);

    for _ in 0..MAX_STARS {
        let x = (lcg_next(&mut seed) % width.max(1)) as u16;
        let y = (lcg_next(&mut seed) % height.max(1)) as u16;
        let brightness = (96 + (lcg_next(&mut seed) % 160)) as u8;
        let twinkle = (lcg_next(&mut seed) & 0x3f) as u8;
        stars.push(Star {
            x,
            y,
            brightness,
            twinkle,
        });
    }

    stars
}

fn frame_slice(video: &VideoLoop, tick: u32) -> Option<&[u8]> {
    let frame_index = (tick % video.frames) as usize;
    let frame_bytes = video.width.checked_mul(video.height)?.checked_mul(4)? as usize;
    let start = LRAW_HEADER_SIZE.checked_add(frame_index.checked_mul(frame_bytes)?)?;
    let end = start.checked_add(frame_bytes)?;
    video.data.get(start..end)
}

fn render_video(engine: &WallpaperEngine, video: &VideoLoop, pixels: &mut [u8], stride: u32) -> bool {
    let Some(src) = frame_slice(video, engine.tick) else {
        return false;
    };

    let dst_w = engine.width as usize;
    let dst_h = engine.height as usize;
    let src_w = video.width as usize;
    let src_h = video.height as usize;
    let stride = stride as usize;

    if dst_w == 0 || dst_h == 0 || src_w == 0 || src_h == 0 || stride < dst_w * 4 {
        return false;
    }

    let use_width_scale = dst_w.saturating_mul(src_h) >= dst_h.saturating_mul(src_w);
    let scaled_w;
    let scaled_h;
    if use_width_scale {
        scaled_w = dst_w;
        scaled_h = dst_w.saturating_mul(src_h).max(src_h) / src_w.max(1);
    } else {
        scaled_h = dst_h;
        scaled_w = dst_h.saturating_mul(src_w).max(src_w) / src_h.max(1);
    }

    let crop_x = scaled_w.saturating_sub(dst_w) / 2;
    let crop_y = scaled_h.saturating_sub(dst_h) / 2;

    for dy in 0..dst_h {
        let sy = if use_width_scale {
            ((dy + crop_y).saturating_mul(src_h) / scaled_h.max(1)).min(src_h - 1)
        } else {
            (dy.saturating_mul(src_h) / dst_h.max(1)).min(src_h - 1)
        };

        let dst_row_start = dy.saturating_mul(stride);
        let dst_row_end = dst_row_start.saturating_add(dst_w * 4);
        let Some(dst_row) = pixels.get_mut(dst_row_start..dst_row_end) else {
            return false;
        };

        for dx in 0..dst_w {
            let sx = if use_width_scale {
                (dx.saturating_mul(src_w) / dst_w.max(1)).min(src_w - 1)
            } else {
                ((dx + crop_x).saturating_mul(src_w) / scaled_w.max(1)).min(src_w - 1)
            };

            let src_idx = (sy.saturating_mul(src_w).saturating_add(sx)).saturating_mul(4);
            let dst_idx = dx.saturating_mul(4);
            if let (Some(src_px), Some(dst_px)) =
                (src.get(src_idx..src_idx + 4), dst_row.get_mut(dst_idx..dst_idx + 4))
            {
                dst_px.copy_from_slice(src_px);
            }
        }
    }

    true
}

fn fill_u32(row: &mut [u8], width: usize, color: u32) {
    let bytes = color.to_le_bytes();
    for px in 0..width {
        let i = px * 4;
        row[i..i + 4].copy_from_slice(&bytes);
    }
}

fn write_pixel(pixels: &mut [u8], stride: usize, width: usize, height: usize, x: usize, y: usize, color: u32) {
    if x >= width || y >= height {
        return;
    }
    let offset = y.saturating_mul(stride).saturating_add(x.saturating_mul(4));
    if let Some(dst) = pixels.get_mut(offset..offset + 4) {
        dst.copy_from_slice(&color.to_le_bytes());
    }
}

fn render_fallback(engine: &WallpaperEngine, pixels: &mut [u8], stride: u32) {
    let width = engine.width as usize;
    let height = engine.height as usize;
    let stride = stride as usize;
    if width == 0 || height == 0 || stride < width * 4 {
        return;
    }

    let (col_top, col_mid, col_bot) = match engine.tod {
        WallpaperTod::Morning => (0x1a1a2e, 0x16213e, 0x0f3460),
        WallpaperTod::Afternoon => (0x0f3460, 0x533483, 0x1b1b2f),
        WallpaperTod::Night => (0x0a0a0f, 0x0f0e15, 0x1b1b2f),
    };

    for y in 0..height {
        let t = ((y as u32) * 255) / engine.height.max(1);
        let mut color = if t < 128 {
            blend_colour(col_top, col_mid, t * 2)
        } else {
            blend_colour(col_mid, col_bot, (t - 128) * 2)
        };

        let wave = ((y as u32) + engine.tick / 4) % engine.height.max(1);
        let band = engine.height / 3;
        if matches!(engine.tod, WallpaperTod::Night | WallpaperTod::Morning)
            && wave >= band
            && wave < band + 20
        {
            let nebula = if wave - band < 10 {
                (wave - band) * 3
            } else {
                (20 - (wave - band)) * 3
            };
            color = blend_colour(color, 0x0078b4, nebula.min(255));
        }

        let row_start = y.saturating_mul(stride);
        let row_end = row_start.saturating_add(width * 4);
        if let Some(row) = pixels.get_mut(row_start..row_end) {
            fill_u32(row, width, color);
        }
    }

    if matches!(engine.tod, WallpaperTod::Night | WallpaperTod::Morning) {
        for star in &engine.stars {
            let x = star.x as usize % width.max(1);
            let y = star.y as usize % height.max(1);
            let phase = ((engine.tick / 2) + star.twinkle as u32) & 0x3f;
            let twinkle = if phase < 32 { phase * 4 } else { (64 - phase) * 4 };
            let mut brightness = star.brightness as u32;
            brightness = (brightness * (128 + twinkle / 2)) / 255;
            brightness = brightness.min(255);
            write_pixel(pixels, stride, width, height, x, y, (brightness << 16) | (brightness << 8) | brightness);
        }
    }
}

impl WallpaperEngine {
    fn new(width: u32, height: u32) -> Self {
        let video = load_video_loop(Path::new(VIDEO_FILE_PATH));
        if let Some(v) = &video {
            println!(
                "luna-shell: Rust wallpaper loaded {}: {}x{} ({} frames)",
                VIDEO_FILE_PATH, v.width, v.height, v.frames
            );
        } else {
            println!(
                "luna-shell: Rust wallpaper using procedural fallback; {} not available",
                VIDEO_FILE_PATH
            );
        }

        Self {
            tick: 0,
            width,
            height,
            tod: current_tod(),
            stars: generate_stars(width, height),
            video,
        }
    }

    fn tick(&mut self) {
        self.tick = self.tick.wrapping_add(1);

        if self.video.is_none() && self.tick % 600 == 0 {
            self.tod = current_tod();
        }

        if self.tick % 60 != 0 {
            return;
        }

        let path = Path::new(VIDEO_FILE_PATH);
        let meta = match fs::metadata(path) {
            Ok(meta) => meta,
            Err(_) => {
                if self.video.is_some() {
                    self.video = None;
                    self.tod = current_tod();
                }
                return;
            }
        };

        let modified = modified_secs(&meta);
        let byte_len = meta.len();
        let changed = self
            .video
            .as_ref()
            .map(|v| v.modified_secs != modified || v.byte_len != byte_len)
            .unwrap_or(true);

        if changed {
            self.video = load_video_loop(path);
            if self.video.is_none() {
                self.tod = current_tod();
            }
        }
    }

    fn render(&self, pixels: *mut c_void, stride: u32) {
        if pixels.is_null() || stride == 0 || self.width == 0 || self.height == 0 {
            return;
        }

        let byte_len = (stride as usize).saturating_mul(self.height as usize);
        let pixels = unsafe { slice::from_raw_parts_mut(pixels.cast::<u8>(), byte_len) };

        if let Some(video) = &self.video {
            if render_video(self, video, pixels, stride) {
                return;
            }
        }

        render_fallback(self, pixels, stride);
    }
}

#[no_mangle]
pub extern "C" fn wallpaper_init(ws: *mut WallpaperState, width: u32, height: u32) {
    if ws.is_null() {
        return;
    }

    unsafe {
        wallpaper_cleanup(ws);
        let engine = Box::new(WallpaperEngine::new(width, height));
        (*ws).engine = Box::into_raw(engine);
    }
}

#[no_mangle]
pub extern "C" fn wallpaper_tick(ws: *mut WallpaperState) {
    if ws.is_null() {
        return;
    }

    let engine = unsafe { (*ws).engine.as_mut() };
    if let Some(engine) = engine {
        engine.tick();
    }
}

#[no_mangle]
pub extern "C" fn wallpaper_render(ws: *const WallpaperState, pixels: *mut c_void, stride: u32) {
    if ws.is_null() {
        return;
    }

    let engine = unsafe { (*ws).engine.as_ref() };
    if let Some(engine) = engine {
        engine.render(pixels, stride);
    }
}

#[no_mangle]
pub extern "C" fn wallpaper_cleanup(ws: *mut WallpaperState) {
    if ws.is_null() {
        return;
    }

    unsafe {
        let engine = (*ws).engine;
        if !engine.is_null() {
            drop(Box::from_raw(engine));
            (*ws).engine = ptr::null_mut();
        }
    }
}

#[no_mangle]
pub extern "C" fn wallpaper_get_tod() -> WallpaperTod {
    current_tod()
}
