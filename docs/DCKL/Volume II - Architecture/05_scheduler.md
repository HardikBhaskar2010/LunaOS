# LunaOS — Scheduler
**Volume II · Chapter 5**
**Classification:** Core Architecture — Process Scheduling
**Status:** Active · Reference for kernel config and resource management

---

## Purpose

This document describes the LunaOS process scheduling architecture: the kernel scheduler configuration, process priority assignments, cgroup v2 hierarchy, and the policy that ensures interactive workloads (the desktop shell, LGP compositor, LUNA.AI UI) remain responsive under CPU pressure from background tasks (AI inference, package builds, downloads).

---

## Overview

LunaOS runs a diverse process mix. The LGP compositor must deliver consistent frame pacing. The shell must respond to input within human perception thresholds. `luna-ai-d` and Ollama perform inference computations that can saturate CPU cores for seconds at a time. `lpkg` builds can consume all available cores.

Without scheduling policy, a build job or inference run starves the compositor and the desktop stutters. The scheduler document defines the policy that prevents this.

---

## Design Philosophy

Three principles govern scheduling decisions in LunaOS:

**1. Interactive processes are never starved.** The LGP compositor, luna-shell, luna-bar, luna-island, and luna-notif must receive CPU time within one frame budget (16ms at 60Hz, 11ms at 90Hz) even when the system is under maximum CPU load from background tasks.

**2. Background tasks are bounded.** AI inference (Ollama) and package builds (lpkg) are explicitly CPU-intensive but not latency-sensitive. They run in a lower-priority cgroup slice and yield to interactive processes automatically.

**3. User applications are not penalized.** An application opened by the user is not treated as a background task by default. It receives normal scheduling priority. Only explicit infrastructure tasks (AI daemon workers, build jobs) are deprioritized.

---

## Architecture

### Scheduler Selection

The kernel uses the **Completely Fair Scheduler (CFS)** with full preemption enabled (`CONFIG_PREEMPT=y`) and 1kHz timer resolution (`CONFIG_HZ_1000=y`). See `03_linux_architecture.md` for the kernel config rationale.

```
CONFIG_PREEMPT=y          Full kernel preemption — compositor can preempt kernel paths
CONFIG_HZ_1000=y          1ms timer tick — fine-grained scheduling decisions
CONFIG_SCHED_AUTOGROUP=y  Automatic session task grouping
CONFIG_FAIR_GROUP_SCHED=y CFS group scheduling for cgroup slices
```

**Why not PREEMPT_RT?**
PREEMPT_RT (real-time preemption) would provide lower worst-case latency for the compositor. However, it requires applying the RT patch series and significantly increases kernel complexity. This is deferred to v1.5 based on audio/graphics latency testing results. See `03_linux_architecture.md` Open Question 5.

### Cgroup v2 Hierarchy

LunaOS uses cgroup v2 to organize processes into resource-bounded slices. `luna-init` creates and manages this hierarchy at boot.

```
/sys/fs/cgroup/
└── luna.slice                        (LunaOS top-level slice)
    │
    ├── luna-compositor.slice          (LGP compositor — highest priority)
    │     └── lgp-compositor.service
    │
    ├── luna-shell.slice               (Shell layer — high priority)
    │     ├── luna-shell.service
    │     ├── luna-bar.service
    │     ├── luna-island.service
    │     └── luna-notif.service
    │
    ├── luna-session.slice             (User applications — normal priority)
    │     └── (all user-launched applications)
    │
    ├── luna-ai.slice                  (AI layer — below normal priority)
    │     ├── luna-ai-d.service
    │     └── ollama.service
    │
    └── luna-system.slice              (System services — normal priority)
          ├── dbus.service
          ├── networkmanager.service
          └── pipewire.service
```

### CPU Weight Policy

CFS uses `cpu.weight` (range 1–10000, default 100) to allocate proportional CPU shares between cgroup slices.

| Slice | cpu.weight | Relative share |
|---|---|---|
| luna-compositor.slice | 800 | ~40% of available CPU (under contention) |
| luna-shell.slice | 400 | ~20% |
| luna-session.slice | 300 | ~15% |
| luna-system.slice | 300 | ~15% |
| luna-ai.slice | 200 | ~10% |

**How CFS weight works in practice:** CPU weight only takes effect under contention. When the system is idle, any process can use 100% of a CPU core. When multiple slices compete for CPU time, the scheduler allocates time proportional to the weight ratios. A compositor at weight 800 will receive approximately 4× more CPU time than an AI slice at weight 200 during a contested period.

This means Ollama can saturate a CPU core during idle desktop periods. The moment the compositor needs to render a frame, CFS preempts Ollama immediately.

```
TODO:
Decision not yet finalized.
Reason: Specific cpu.weight values above are initial estimates.
They must be validated against real hardware under:
  - Ollama inference load + compositor at 60Hz
  - lpkg build load + compositor at 60Hz
  - Both simultaneously
Values may need adjustment after performance testing.
See: Future Improvements.
```

### CPU Quota for AI Slice

In addition to CPU weight, `luna-ai.slice` has a CPU bandwidth quota to prevent AI inference from holding a CPU core for long uninterrupted bursts:

```
luna-ai.slice:
  cpu.max = "500000 1000000"   # 50% of one CPU core per 1-second period
```

This means Ollama can use at most 50% of one CPU core in any 1-second window, regardless of how many cores are available. The remaining CPU is available for other slices.

```
TODO:
Decision not yet finalized.
Reason: The 50% CPU quota for the AI slice is an initial estimate.
This throttles inference performance. If LUNA.AI responses feel too slow on
reference hardware, this limit should be raised.
Testing required before finalizing this value.
```

### Memory Limits

cgroup v2 also provides memory limits. LunaOS sets soft limits (memory.high) that trigger memory pressure signals, and hard limits (memory.max) that OOM-kill processes within the slice.

| Slice | memory.high | memory.max | Rationale |
|---|---|---|---|
| luna-compositor.slice | 512 MB | 1 GB | Compositor should not OOM the system |
| luna-shell.slice | 256 MB | 512 MB | Shell components are lightweight |
| luna-session.slice | none | none | User applications: no limit |
| luna-system.slice | 256 MB | 512 MB | System daemons are lightweight |
| luna-ai.slice | 5 GB | 6 GB | Ollama holds model weights in RAM |

```
TODO:
Decision not yet finalized.
Reason: AI slice memory limits depend on which Ollama model is loaded.
Phi-3 Mini: ~2 GB. Qwen2.5 3B: ~3 GB.
If both are loaded simultaneously: ~5 GB.
The 6 GB hard limit above assumes 8 GB total RAM.
On 4 GB systems, this limit must be lower (which constrains model choice).
Model selection and minimum RAM spec must be resolved together.
See architecture_overview.md Open Question 4.
```

### Process Priority (nice values)

In addition to cgroup weights, individual processes within a cgroup may be assigned nice values. The LGP compositor is the only LunaOS process that uses a negative nice value:

| Process | nice value | Rationale |
|---|---|---|
| `lgp-compositor` | -10 | Compositor latency is non-negotiable — must preempt everything |
| `luna-shell` | 0 | Normal priority within compositor slice |
| `luna-ai-d` | 5 | Slightly below normal — interactive but not latency-critical |
| `ollama` | 10 | Background task — explicitly deprioritized |
| `lpkg` build jobs | 15 | Build jobs are maximum-background |

```
TODO:
Decision not yet finalized.
Reason: Nice values for lgp-compositor depend on the compositor architecture.
If the compositor uses a dedicated render thread that operates on a fixed vsync
interval, a real-time priority (SCHED_FIFO) may be more appropriate than a
negative nice value. This must be decided during compositor implementation
(Volume III) and the scheduler doc updated accordingly.
```

### I/O Scheduling

CPU scheduling is only part of the picture. Disk I/O must also be bounded to prevent `lpkg` installs or AI model loading from starving the compositor's shader cache reads.

```
TODO:
Decision not yet finalized.
Reason: LunaOS I/O scheduling policy has not been specified.
Relevant options:
  - cgroup v2 io.weight controller (proportional I/O weight, same model as cpu.weight)
  - BFQ I/O scheduler (per-process I/O fairness, good for desktop use)
  - mq-deadline (simpler, lower overhead)
Decision required before v1. Recommend investigating BFQ with io.weight as a
complementary mechanism.
```

---

## Technical Details

### Cgroup Hierarchy Creation

`luna-init` creates the cgroup hierarchy at Stage 2 boot, before any services are started. Each cgroup slice is created by writing to the cgroup v2 filesystem:

```c
// luna-init cgroup setup (pseudocode — C implementation)
mkdir("/sys/fs/cgroup/luna.slice", 0755);
mkdir("/sys/fs/cgroup/luna.slice/luna-compositor.slice", 0755);
// ... etc.

// Set cpu.weight for luna-compositor.slice
write_file("/sys/fs/cgroup/luna.slice/luna-compositor.slice/cpu.weight", "800");

// Set cpu.max for luna-ai.slice (50% of one core per second)
write_file("/sys/fs/cgroup/luna.slice/luna-ai.slice/cpu.max", "500000 1000000");
```

When luna-init starts a service via `fork + execve`, it moves the child PID into the appropriate cgroup slice by writing to `cgroup.procs` before exec():

```c
// After fork(), before execve(), in the child process:
write_to_file("/sys/fs/cgroup/luna.slice/luna-ai.slice/cgroup.procs", getpid());
execve("/usr/bin/ollama", args, env);
```

This ensures every service is in its designated cgroup from the moment it starts.

### Compositor Frame Budget

The LGP compositor must deliver a rendered frame within the vsync deadline. At 60Hz, the frame budget is 16.67ms. At 90Hz, 11.11ms. At 120Hz, 8.33ms.

```
TODO:
Decision not yet finalized.
Reason: Target display refresh rate has not been specified for v1.
60Hz is the safe baseline. 90Hz and 120Hz are stretch goals.
The compositor scheduling model (dedicated render thread, SCHED_FIFO vs. CFS)
must account for the target refresh rate.
This must be decided in Volume III / 03_compositor.md.
```

The Animation Budget from `core_laws.md` Law III establishes the maximum allowed animation durations:

| Interaction type | Max duration | Frames at 60Hz |
|---|---|---|
| Button press / click response | ≤ 100ms | ≤ 6 frames |
| UI element transition | ≤ 200ms | ≤ 12 frames |
| Window open / close | ≤ 300ms | ≤ 18 frames |
| State change animation | ≤ 400ms | ≤ 24 frames |
| Complex narrative motion | ≤ 2000ms | ≤ 120 frames |

The scheduler policy must guarantee that the compositor can produce these frames on time under normal system load.

### SCHED_AUTOGROUP Behavior

With `CONFIG_SCHED_AUTOGROUP=y`, the kernel automatically creates a task group for each session (login session, process group). This means:

- A build job running in a terminal automatically gets its own task group.
- That task group competes with other task groups (including the compositor group) at the CFS level.
- Without additional configuration, this already provides significant isolation between user applications and background tasks.

The explicit cgroup policy above builds on top of autogroup — it provides finer-grained control and memory limits that autogroup does not handle.

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| PREEMPT_RT evaluation | v1.5 | Measure compositor latency under Ollama load; decide RT patch necessity |
| cpu.weight tuning | v1 | Validate initial weight values under real workloads |
| BFQ I/O scheduler adoption | v1 | Pair with io.weight for complete CPU+I/O resource control |
| Per-app cgroup assignment | v1.5 | Luna shell classifies launched applications into appropriate slices |
| SCHED_FIFO for compositor render thread | v1.5 | If CFS scheduling produces unacceptable frame time variance |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Target display refresh rate.** 60Hz, 90Hz, or 120Hz baseline? This affects the frame budget calculation and compositor scheduling requirements.

2. **CPU quota for AI slice (50%).** Initial estimate. Must be tested against real inference workloads. May need to be raised if LUNA.AI response times are unacceptably slow.

3. **I/O scheduling policy.** No decision made. BFQ + io.weight is recommended. Must be a Decision Log entry before v1.

4. **Compositor render thread scheduling class.** SCHED_FIFO vs. CFS with negative nice value. Depends on compositor architecture decisions in Volume III.

5. **AI model memory allocation.** Ollama's RAM usage depends on model choice. The 6 GB memory.max for luna-ai.slice assumes 8 GB total RAM and a specific model. Both assumptions need to be formalized.

---

## AI Context

An AI agent implementing LunaOS scheduling must understand:

- Cgroup v2 is the resource management mechanism. Cgroup v1 is not used.
- `luna-init` creates the cgroup hierarchy and assigns each service to a slice at start time. No service assigns itself.
- The cgroup hierarchy has five slices: compositor, shell, session, AI, and system. Their relative cpu.weight values are: 800, 400, 300, 200, 300.
- The AI slice (luna-ai.slice) has an additional `cpu.max` quota of 50% of one CPU core per second. This throttles inference but protects interactive responsiveness.
- The LGP compositor runs at nice -10. This is the only LunaOS process with a negative nice value.
- Ollama runs at nice +10. It is explicitly a background task.
- `lpkg` build jobs run at nice +15. They must never impact desktop responsiveness.
- All scheduling policy values above are initial estimates pending validation on reference hardware.
- PREEMPT_RT is not used in v1. The decision to use it (or not) is deferred to v1.5 based on measured latency data.

---

*Document: `Volume II / 05_scheduler.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: architecture_overview.md, boot_flow.md, linux_architecture.md, init_system.md, core_laws.md (Law III Animation Budget), non_negotiables.md*
