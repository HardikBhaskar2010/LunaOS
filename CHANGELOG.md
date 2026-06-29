# Changelog

## v0.1

- Initial bootloader
- luna-init
- Service Manager
- TOML parser
- Dependency graph
- Framebuffer console

## v0.2

- luna-splash
- Boot progress

## Unreleased

- Started Phase 2 graphics bring-up with `lgp-compositor` DRM/KMS setup, LGP socket lifecycle, 6-byte TLV framing per DL-053, and `LGP_HELLO` handling.
- Fixed compositor client lifecycle so handshaken clients remain tracked until disconnect.
- Added bounded LGP stream reassembly so split socket reads are buffered and parsed as complete TLV frames.
- Added the Phase 2 minimum LGP surface path: `LGP_CREATE_SURFACE`, `LGP_DESTROY_SURFACE`, `LGP_COMMIT_BUFFER`, compositor-owned surface state, `SCM_RIGHTS` shared-memory commits, and a direct-LGP test client.
- Added LGP protocol/surface unit tests and TLV fuzz coverage.
- Documented Mahina setup as GUI-first with terminal setup reserved for emergency and development use.
- Fixed TOML array validation, dependency graph cycle validation, and the clean-tree unit/fuzz test targets.
