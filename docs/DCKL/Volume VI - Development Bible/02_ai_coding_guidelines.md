# Mahina — AI Coding Guidelines
**Volume VI · Chapter 02**
**Classification:** Development Bible — Process Standards
**Status:** Canonical

---

## Purpose

This document defines the rules for AI coding agents (Claude Code, Codex, GitHub Copilot) operating within the Mahina repository. 

AI agents are powerful contributors, but they require strict architectural constraints to prevent drift, maintain the "documentation is code" philosophy, and preserve the vision of Mahina.

---

## The Prime Directive

An AI coding agent operating in the Mahina repository must **never invent architecture**.

If a feature requires a structural decision (e.g., choosing a protocol, selecting an IPC mechanism, defining a new layer, allocating a new cgroup), the AI must:
1. Stop coding.
2. Check `Volume I / decision_log.md` for an existing decision.
3. If no decision exists, prompt the human Principal Engineer to make one.
4. Document the new decision before writing code.

---

## Autonomous vs. Manual Execution

### What AI Can Do Autonomously
- **Write implementation:** Implement C code that adheres to `01_coding_standards.md` based on an accepted architecture document.
- **Refactor within bounds:** Extract functions, optimize tight loops, and clean up memory leaks, provided the public API and IPC surface do not change.
- **Generate tests:** Write unit tests for pure functions.
- **Update documentation:** Sync implementation details (e.g., struct fields) back to the relevant Volume II–V documents.

### What Requires Human Review (Strictly Blocked)
- **Adding dependencies:** Linking a new C library or requiring a new system binary.
- **Changing IPC:** Adding a new D-Bus interface or modifying the LGP wire format.
- **Modifying Ownership:** Changing which process owns a capability (defined in `Volume II / 13_component_ownership.md`).
- **Altering the Boot Flow:** Modifying `luna-init` stages.
- **Changing memory limits:** Modifying cgroup configurations or `OOM_SCORE_ADJ`.

---

## Required Workflow for AI Agents

1. **Context Check:** Always read `Volume I / core_laws.md` and `Volume I / philosophy.md` before beginning a complex task.
2. **Architecture Sync:** Verify the task against the relevant Volume II–V document.
3. **Execution:** Write code.
4. **Validation:** Ensure no `TODO: Decision not yet finalized` blocks were bypassed.

---

*Document: `Volume VI / 02_ai_coding_guidelines.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Status: Active*
