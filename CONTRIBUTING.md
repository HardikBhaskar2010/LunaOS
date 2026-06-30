# Contributing to Mahina OS

Thank you for your interest in contributing to Mahina OS! Building an operating system is an immensely complex task, and we deeply value developers who want to help make Mahina cleaner, faster, and more beautiful.

## 📖 Core Philosophy: Documentation-First Engineering

Before contributing any code, please read the entire Divine Collection of Knowledge about Luna (DCKL) in the `docs/DCKL/` directory. 

Mahina strictly follows **Documentation-First Engineering**. 
No code is merged unless the architectural decisions have been formally accepted in the Decision Log. If you write a feature without documenting its architecture in the DCKL first, your Pull Request **will be closed**.

Specifically, you must read and understand:
1. `docs/DCKL/Volume I - Foundation/core_laws.md`
2. `docs/DCKL/Volume VI - Development Bible/01_coding_standards.md`

## 🛠️ Getting Started

1. **Check the Roadmap:** Look at the `Volume VII - Implementation Roadmap` to see what is currently in scope. Do not submit PRs for v2.0 features when we are currently building v0.x. Focus on the active milestones.
2. **Clone the Repository:**
   ```bash
   git clone https://github.com/HardikBhaskar2010/MahinaOS.git
   cd MahinaOS
   ```
3. **Compile and Test:**
   - Ensure your C17 code compiles with zero warnings under `clang` (`-Wall -Wextra -Werror`).
   - Run `make lint` to run `clang-tidy` across the codebase.
   - Run `make test-unit` to ensure there are no memory leaks (ASan and UBSan are enabled by default in test builds).

## 🌳 Branching Strategy

We use a simple feature-branch workflow.
- `main` is the stable, protected branch. It must always compile and boot.
- Create feature branches off `main` using the following naming convention:
  - `feat/feature-name`
  - `fix/bug-name`
  - `docs/document-name`

## ✉️ Commit Message Format

We follow the Conventional Commits specification. This allows us to automatically generate `CHANGELOG.md`.

```
<type>(<scope>): <subject>

<body>
```

**Types:**
- `feat:` A new feature or architectural addition.
- `fix:` A bug fix.
- `docs:` Changes exclusively to the DCKL or repository markdown files.
- `refactor:` Code changes that neither fix a bug nor add a feature.
- `perf:` A code change that improves performance.
- `test:` Adding missing tests or correcting existing tests.
- `build:` Changes that affect the build system (`Makefile`).

**Example:**
```
feat(luna-init): add cycle detection to dependency graph

Implemented Tarjan's strongly connected components algorithm to detect circular dependencies during Stage 4 service resolution.
```

## 🚀 Submitting Pull Requests

Every PR should include:
- **Description:** A clear explanation of what the PR accomplishes.
- **DCKL Alignment:** A link to the specific Volume/Chapter in the DCKL that this PR fulfills.
- **Documentation Updates:** If you changed how a system works, update the corresponding markdown file in the DCKL.
- **Tests:** Add unit tests to `tests/unit/` for any new C code.
- **Screenshots:** If your change affects visuals (e.g., `luna-splash`), include a screenshot or QEMU recording in the PR description.
- **Copyright Header:** Ensure you include the standard copyright header in all new `.c` and `.h` files.

Thank you for helping us build Mahina!
