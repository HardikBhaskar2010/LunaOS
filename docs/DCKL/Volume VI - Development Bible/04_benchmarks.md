# Mahina — Benchmarks & Performance Targets
**Volume VI · Chapter 04**
**Classification:** Development Bible — Metrics
**Status:** Canonical

---

## Purpose

This document defines the quantitative performance targets for Mahina. 

If a subsystem fails to meet these benchmarks on reference hardware, it is considered a bug and blocks the release.

---

## Reference Hardware (Minimum Spec)

These targets apply to the lowest supported hardware tier (v1 target):
- **CPU:** Intel Core i5 (8th Gen, 4 cores) or AMD equivalent
- **RAM:** 8 GB
- **GPU:** Integrated Graphics (Intel UHD 620)
- **Disk:** SATA SSD

---

## 1. Boot Performance

The time from UEFI handoff (bootloader exit) to an interactive desktop.

- **Target:** ≤ 5 seconds
- **Critical Path:** `luna-init` execution, DRM initialization, compositor start, `luna-shell` start.
- **Constraints:**
  - AI model loading is explicitly excluded from the boot critical path (lazy loaded).
  - NetworkManager initialization is asynchronous and does not block the desktop.

---

## 2. Graphics & Compositor

The compositor is the most performance-sensitive component.

- **Target Frame Rate:** 60 FPS (16.6ms per frame) solid.
- **Frame Drop Budget:** ≤ 1 dropped frame per 10,000 frames under standard desktop load.
- **Compositor CPU Usage:** ≤ 2% on reference hardware when idle (only clock updating).
- **Animation Latency:** The time from input event (e.g., click) to the first frame of animation must be ≤ 32ms (2 frames).

---

## 3. AI Runtime (LUNA)

AI performance is split between the Presence Engine (always on) and the Inference Engine (LLM, lazy loaded).

### Presence Engine
- **Memory Footprint:** ≤ 200 MB (resident set size).
- **Context Switch Latency:** When the user switches applications, LUNA mode must update within 500ms.

### Inference Engine (Default Model: Phi-3 Mini 4-bit)
- **Time to First Token (Cold Start):** ≤ 5 seconds (includes spawning Ollama and loading model from SSD to RAM).
- **Time to First Token (Warm):** ≤ 2 seconds.
- **Sustained Generation:** ≥ 3 tokens/second on CPU.

---

## 4. Resource Footprint

The base system must leave room for the AI model and user applications.

- **System RAM Footprint (Idle):** ≤ 1.5 GB (excluding the LLM).
- **Disk Footprint (Base OS):** ≤ 8 GB (including the default AI model).

---

*Document: `Volume VI / 04_benchmarks.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Status: Active*
