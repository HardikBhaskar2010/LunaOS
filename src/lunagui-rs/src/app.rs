use lgp::connection::LgpConnection;
use lgp::protocol::*;
use lgp::surface::LgpSurface;
use lgp::input::*;

pub enum AppEvent {
    PointerMotion { x: f64, y: f64 },
    PointerButton { x: f64, y: f64, button: u32, pressed: bool },
    Keyboard { key: u32, modifiers: u32, pressed: bool },
    SurfaceCreated { surface_id: u32, surface_type: u32, width: u32, height: u32 },
    SurfaceDestroyed { surface_id: u32 },
    FocusChanged { surface_id: u32 },
    Timer,
    Custom(u32, Vec<u8>),
}

pub trait AppHandler {
    fn init(&mut self, conn: &mut LgpConnection, width: u32, height: u32);
    fn render(&mut self, pixels: &mut [u8], stride: u32, width: u32, height: u32);
    fn handle_event(&mut self, event: &AppEvent, conn: &mut LgpConnection);
    fn on_pointer_motion(&mut self, _x: f64, _y: f64, _conn: &mut LgpConnection) {}
    fn on_pointer_button(&mut self, _x: f64, _y: f64, _button: u32, _pressed: bool, _conn: &mut LgpConnection) {}
    fn on_key(&mut self, _key: u32, _modifiers: u32, _pressed: bool, _conn: &mut LgpConnection) {}
    fn on_timer(&mut self, _conn: &mut LgpConnection) {}
}

pub struct App {
    pub conn: LgpConnection,
    pub surface: Option<LgpSurface>,
    pub width: u32,
    pub height: u32,
    pub running: bool,
    pub handler: Option<Box<dyn AppHandler>>,
    pub cursor_x: f64,
    pub cursor_y: f64,
}

impl App {
    pub fn new(caps_requested: u32) -> std::io::Result<Self> {
        let conn = LgpConnection::connect(caps_requested)?;
        let width = conn.output_width;
        let height = conn.output_height;
        Ok(Self {
            conn,
            surface: None,
            width,
            height,
            running: true,
            handler: None,
            cursor_x: (width / 2) as f64,
            cursor_y: (height / 2) as f64,
        })
    }

    pub fn set_handler(&mut self, mut handler: Box<dyn AppHandler>) {
        handler.init(&mut self.conn, self.width, self.height);
        self.handler = Some(handler);
    }

    pub fn create_surface(&mut self, surface_type: u32, layer: u32, argb: bool, x: i32, y: i32) -> std::io::Result<()> {
        let surface = LgpSurface::create(
            &mut self.conn,
            surface_type,
            x, y,
            self.width, self.height,
            layer,
            argb,
        )?;
        self.surface = Some(surface);
        Ok(())
    }

    pub fn present(&mut self) -> std::io::Result<()> {
        if let Some(ref mut surface) = self.surface {
            if let Some(ref mut handler) = self.handler {
                let width = surface.width;
                let height = surface.height;
                let pixels = surface.pixels();
                let stride = width * 4;
                handler.render(pixels, stride, width, height);
            }
            surface.commit(&mut self.conn)?;
        }
        Ok(())
    }

    pub fn run(&mut self) -> std::io::Result<()> {
        self.conn.set_nonblocking(false)?;

        let mut pending_events = Vec::new();

        while self.running {
            match self.conn.recv() {
                Ok(msg) => {
                    self.dispatch_message(&msg, &mut pending_events);
                }
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {}
                Err(e) if e.kind() == std::io::ErrorKind::Interrupted => {}
                Err(e) => {
                    eprintln!("Connection error: {}", e);
                    break;
                }
            }

            for event in pending_events.drain(..) {
                if let Some(ref mut handler) = self.handler {
                    handler.handle_event(&event, &mut self.conn);
                }
            }
        }
        Ok(())
    }

    fn dispatch_message(&mut self, msg: &LgpMessage, events: &mut Vec<AppEvent>) {
        match msg.msg_type {
            t if t == LgpMessageType::PointerMotion as u16 => {
                if let Some(ev) = parse_pointer_motion(&msg.payload) {
                    self.cursor_x = ev.x;
                    self.cursor_y = ev.y;
                    events.push(AppEvent::PointerMotion { x: ev.x, y: ev.y });
                    if let Some(ref mut handler) = self.handler {
                        handler.on_pointer_motion(ev.x, ev.y, &mut self.conn);
                    }
                }
            }
            t if t == LgpMessageType::PointerButton as u16 => {
                if let Some(ev) = parse_pointer_button(&msg.payload) {
                    events.push(AppEvent::PointerButton {
                        x: self.cursor_x, y: self.cursor_y, button: ev.button, pressed: ev.pressed,
                    });
                    if let Some(ref mut handler) = self.handler {
                        handler.on_pointer_button(self.cursor_x, self.cursor_y, ev.button, ev.pressed, &mut self.conn);
                    }
                }
            }
            t if t == LgpMessageType::KeyboardKey as u16 => {
                if let Some(ev) = parse_keyboard_key(&msg.payload) {
                    events.push(AppEvent::Keyboard {
                        key: ev.key, modifiers: ev.modifiers, pressed: ev.pressed,
                    });
                    if let Some(ref mut handler) = self.handler {
                        handler.on_key(ev.key, ev.modifiers, ev.pressed, &mut self.conn);
                    }
                }
            }
            t if t == LgpMessageType::WmSurfaceCreated as u16 => {
                if msg.payload.len() >= 16 {
                    let id = read_u32_le(&msg.payload, 0);
                    let st = read_u32_le(&msg.payload, 4);
                    let w = read_u32_le(&msg.payload, 8);
                    let h = read_u32_le(&msg.payload, 12);
                    events.push(AppEvent::SurfaceCreated {
                        surface_id: id, surface_type: st, width: w, height: h,
                    });
                }
            }
            t if t == LgpMessageType::WmSurfaceDestroyed as u16 => {
                if msg.payload.len() >= 4 {
                    let id = read_u32_le(&msg.payload, 0);
                    events.push(AppEvent::SurfaceDestroyed { surface_id: id });
                }
            }
            t if t == LgpMessageType::WmSetFocus as u16 => {
                if msg.payload.len() >= 4 {
                    let id = read_u32_le(&msg.payload, 0);
                    events.push(AppEvent::FocusChanged { surface_id: id });
                }
            }
            _ => {
                events.push(AppEvent::Custom(msg.msg_type as u32, msg.payload.clone()));
            }
        }
    }
}
