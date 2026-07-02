use lgp::connection::LgpConnection;
use lgp::surface::LgpSurface;

pub struct Window {
    pub surface: Option<LgpSurface>,
    pub width: u32,
    pub height: u32,
    pub x: i32,
    pub y: i32,
}

impl Window {
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            surface: None,
            width,
            height,
            x: 0,
            y: 0,
        }
    }

    pub fn create_surface(&mut self, conn: &mut LgpConnection, surface_type: u32, layer: u32, argb: bool) -> std::io::Result<()> {
        let surface = LgpSurface::create(conn, surface_type, self.x, self.y, self.width, self.height, layer, argb)?;
        self.surface = Some(surface);
        Ok(())
    }

    pub fn commit(&self, conn: &mut LgpConnection) -> std::io::Result<()> {
        if let Some(ref surface) = self.surface {
            surface.commit(conn)
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::NotConnected, "no surface"))
        }
    }

    pub fn pixels(&mut self) -> Option<&mut [u8]> {
        self.surface.as_mut().map(|s| s.pixels())
    }

    pub fn buffer_size(&self) -> usize {
        self.surface.as_ref().map_or(0, |s| s.buffer_size)
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        // Surface is cleaned up in LgpSurface::drop
    }
}
