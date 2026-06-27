# Mahina — Contributing Guidelines
**Volume VI · Chapter 08**
**Classification:** Development Bible — Community
**Status:** Canonical

---

## Welcome to Mahina

Mahina is an ambitious project: an operating system built from scratch, designed around local AI presence, running on Linux.

We welcome contributions, but because we are building a cohesive system rather than a collection of disparate tools, we have strict architectural rules. This document explains how to contribute successfully.

---

## 1. Understand the Vision

Before writing code, read:
1. `Volume I / 04_core_laws.md`
2. `Volume I / 02_philosophy.md`
3. `Volume VI / 01_coding_standards.md`

Code that works but violates the Core Laws will not be merged. (e.g., A pull request that adds a mandatory cloud service dependency will be rejected under Law I.)

---

## 2. The Decision Log

Architecture is not debated in pull requests. It is decided in the **Decision Log** (`Volume I / decision_log.md`).

If you want to introduce a new technology (e.g., "Let's use Wayland instead of LGP" or "Let's use systemd instead of luna-init"), you must first submit a proposal to amend the Decision Log. Only after a decision is accepted should implementation begin.

---

## 3. How to Contribute

### Finding Work
Look at `Volume VII / implementation_roadmap.md`. This is the living checklist of what needs to be built next. Pick an item marked `[ ] Not started` that is not blocked by previous stages.

### Making Changes
1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/luna-init-stage2`).
3. Write your code following `01_coding_standards.md`.
4. Update the relevant documentation. **Documentation is code.** If you add a field to a struct, update the Volume II–V document that specifies that struct.
5. Submit a Pull Request.

### Pull Request Requirements
- Must compile without warnings on the target toolchain.
- Must not introduce memory leaks (run your code under Valgrind/ASan if applicable).
- Must include updates to documentation if architecture was modified.

---

## 4. Design Over Features

We prefer a feature to be missing rather than poorly designed. 
Do not submit "placeholder" UI. If you are building a LunaGUI component, it must adhere to the visual language defined in `Volume III / 09_visual_language.md`.

---

*Document: `Volume VI / 08_contributing.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Status: Active*
