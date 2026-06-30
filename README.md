<div align="center">
  <h1>Mahina OS</h1>
  <p><i>A modern, lightweight, and AI-native operating system built on Documentation-First Engineering.</i></p>
</div>

---

## 🌙 What is Mahina?

Mahina OS is a ground-up initiative to build a clean, deterministic, and highly responsive operating system. Rather than piling on decades of legacy cruft, Mahina is built with strict, documented architectural constraints from day one.

It aims to provide:
1. **Uncompromising Determinism:** Through our custom `luna-init` service manager.
2. **Beautiful Aesthetics:** Starting from the very first frame of the bootloader (`luna-splash`).
3. **Deep AI Integration:** A system designed to be operated natively by intelligent agents.

## ✨ Features

- **`luna-init`:** A completely custom PID 1 init system. It uses TOML for service definitions, builds a deterministic dependency graph, detects cycles, and reaps zombies gracefully.
- **`luna-splash`:** A decoupled boot graphics engine that paints the screen directly via the Linux framebuffer (`/dev/fb0`), completely free of dynamic memory allocation (`malloc`).
- **LGP (Luna Graphics Protocol):** A modern, minimal display protocol designed to replace heavy legacy systems, complete with surface management, alpha blending, and privileged Window Manager extensions.
- **LunaGUI Toolkit:** A lightweight native GUI library in C17 featuring a robust widget tree, custom layouts (VBox/HBox), scrolling, and event-routing.
- **luna-shell:** A native desktop shell and Window Manager.
- **Documentation-First:** No code is written unless it is first codified in the Divine Collection of Knowledge about Luna (DCKL).

## 🏗️ Architecture

Mahina's architecture is explicitly defined in the `docs/DCKL/` directory. If you want to understand how the system works, start there. The code exists solely to satisfy the documentation.

### Core Components
- **Kernel:** Custom-configured Linux kernel.
- **Bootloader:** Limine.
- **Init System:** `luna-init` (Custom C17).
- **Boot Splash:** `luna-splash` (Custom C17).
- **Compositor:** `lgp-compositor` (Custom C17).
- **Desktop Shell:** `luna-shell` (Custom C17).

## 🛠️ Build Instructions

Mahina uses a standard Makefile toolchain. You must be on Linux or WSL2 to build the final disk image.

### Prerequisites
- Clang/LLVM toolchain
- `make`
- QEMU (for virtualization testing)
- `mtools`, `xorriso`, `parted` (for ISO/IMG generation)

### Compiling

```bash
# Build all Mahina binaries
make all

# Run static analysis and linting (Required)
make lint

# Run unit tests with ASan and UBSan
make test-unit

# Build the disk image and boot it in QEMU
make run-qemu
```

## 🗺️ Roadmap

Mahina is currently in the **Phase 3: AI & Shell Integration** stage. We have successfully implemented a deterministic init system, boot graphics, the compositor, the native LunaGUI widget toolkit, a window manager shell, and ten desktop applications.

See [ROADMAP.md](ROADMAP.md) for a high-level overview, or dive into `docs/DCKL/Volume VII - Implementation Roadmap/` for specific engineering milestones.

## 🤝 Contributing

We welcome contributions, but we have strict guidelines to maintain architectural purity. 
Please read [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before submitting a Pull Request.

## 📜 License

Mahina OS is licensed under the MIT License. See [LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md) for details.
