# Mahina OS

![Mahina Logo](logo.png)

## What is Mahina?

Mahina OS is a modern, lightweight operating system designed for simplicity, speed, and clean architecture. Built strictly according to Documentation-First Engineering, Mahina guarantees that its architecture matches its implementation perfectly.

## Features

- **luna-init**: A deterministic, dependency-graph based init system.
- **luna-splash**: Decoupled, zero-malloc boot graphics engine.
- **LGP (Luna Graphics Protocol)**: A modern display protocol and compositor (In Development).

## Architecture

Please review our comprehensive Document Control Knowledge Library (DCKL) located in `docs/DCKL/` for the complete architectural specification.

## Build Instructions

```bash
make all
make run-qemu
```

## Documentation

See the `docs/` folder for architectural decisions, roadmaps, and the full DCKL.

## Roadmap

See `ROADMAP.md` for a high-level overview or `docs/DCKL/Volume VII - Implementation Roadmap/` for specific engineering milestones.

## Contributing

We welcome contributions! See `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md`.

## License

MIT License. See `LICENSE` and `COPYRIGHT.md` for details.
