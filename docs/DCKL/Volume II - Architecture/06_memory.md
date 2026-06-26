# LunaOS — Memory Architecture
**Volume II · Chapter 6**
**Classification:** Core Architecture — Memory Management
**Status:** Active · Reference for kernel config and service implementation

---

## Purpose

This document describes how LunaOS manages memory across the system: physical memory layout, virtual address space policies, swap configuration, LUNA.AI memory data store, and per-process memory constraints enforced through the cgroup v2 hierarchy described in `05_scheduler.md`.

This document covers:
- Physical and virtual memory layout decisions
- Swap strategy (zswap, zram, disk swap)
- The LUNA.AI memory subsystem (`~/.luna/memory/`)
- Memory pressure handling
- Per-slice memory limits from the scheduler cgroup hierarchy

---

## Overview

LunaOS has two distinct memory concerns that must not be confused:

1. **System memory management** — how the Linux kernel and LunaOS infrastructure allocate, protect, and reclaim physical RAM for all running processes.

2. **LUNA.AI memory** — the persistent user-space data store in `~/.luna/memory/` that records workflow patterns, user preferences, and interaction history for the AI layer. This is not OS memory. It is a structured data store with privacy and ownership guarantees under Core Law II.

Both are documented here because both are called "memory" in different contexts. They are completely separate systems.

---

## Design Philosophy

Memory management in LunaOS is governed by two Core Laws:

**Law I (Own Every Layer):** Every memory-related kernel option is explicitly chosen. No default configuration is inherited without acknowledgment. The kernel's OOM killer policy, transparent hugepage behavior, and swap configuration are all deliberate decisions.

**Law II (Local First — Privacy Sub-Law):** The LUNA.AI memory data store in `~/.luna/` is owned exclusively by the user. It is never read by any process except `luna-ai-d`. It is never transmitted anywhere without explicit user instruction. `luna memory --clear` always works and always wipes it completely.

---

## Architecture

### Physical Memory Regions

Physical Memory — 8 GB total (at boot, before first LLM demand)

┌──────────────────────────────────────────┐  High address
│  User applications                          │  ~1.5 GB (variable)
├──────────────────────────────────────────┤
│  LGP compositor + shell                     │  ~512 MB
├──────────────────────────────────────────┤
│  System services (D-Bus, NetworkMgr,        │  ~256 MB
│  PipeWire, luna-ai-d Presence Engine)       │
├──────────────────────────────────────────┤
│  Page cache (filesystem I/O cache)          │  ~4 GB (dynamic — expands to fill free RAM)
├──────────────────────────────────────────┤
│  Kernel memory (kmalloc, vmalloc,           │  ~256 MB
│  per-CPU data, kernel stacks)               │
└──────────────────────────────────────────┘  Low address

DL-021 NOTE: Ollama model weights are NOT present at boot. Per DL-021, the LLM
Inference Engine is lazy-loaded on first demand. On a system idle at desktop,
the ~3 GB Ollama weight region does not exist. That space is page cache.

After first LLM demand (physical memory layout changes):

┌──────────────────────────────────────────┐  High address
│  Ollama model weights (resident once loaded) │  ~3 GB
├──────────────────────────────────────────┤
│  User applications                          │  ~1.5 GB (variable)
├──────────────────────────────────────────┤
│  LGP compositor + shell                     │  ~512 MB
├──────────────────────────────────────────┤
│  System services + Presence Engine          │  ~256 MB
├──────────────────────────────────────────┤
│  Page cache (compressed to free space)      │  ~1 GB (shrunken by Ollama)
├──────────────────────────────────────────┤
│  Kernel memory                              │  ~256 MB
└──────────────────────────────────────────┘  Low address

The dominant memory consumer is Ollama's model weights held in RAM for fast inference. This is unavoidable with local AI inference. The system is designed around this constraint.

### Virtual Address Space

LunaOS uses the standard Linux x86_64 virtual address space layout:

```
0xFFFF FFFF FFFF FFFF   (top of virtual address space)
        Kernel space     (128 TB — KASLR randomized)
0xFFFF 8000 0000 0000
        ...gap...
0x0000 7FFF FFFF FFFF   (top of user space)
        User space       (128 TB)
0x0000 0000 0000 0000
```

KASLR (`CONFIG_RANDOMIZE_BASE=y`) is enabled. The kernel base address is randomized at each boot. This is a non-optional security requirement.

User-space ASLR is enabled by default (`/proc/sys/kernel/randomize_va_space = 2`). All LunaOS processes run with full ASLR. This is configured in `/etc/luna/sysctl.toml`:

```toml
# /etc/luna/sysctl.toml
[kernel]
randomize_va_space = 2       # Full ASLR for all processes
kptr_restrict      = 2       # Hide kernel pointers from unprivileged users
dmesg_restrict     = 1       # Restrict dmesg access to root
```

### Transparent Hugepages

`CONFIG_TRANSPARENT_HUGEPAGE_MADVISE=y` is set, meaning transparent hugepages are used only when a process explicitly requests them via `madvise(addr, len, MADV_HUGEPAGE)`.

Ollama and luna-ai-d are the primary users of hugepages. AI inference workloads access large contiguous memory regions (model weights, key-value caches). Hugepages reduce TLB pressure during inference, improving throughput.

The default (not always-on) mode is chosen to avoid the memory waste and latency spikes that always-on hugepages cause for small, allocation-heavy processes like the shell.

### Swap Configuration

LunaOS uses a two-tier swap strategy:

**Tier 1 — zswap (RAM-compressed swap cache):**
- `CONFIG_ZSWAP=y`
- Compresses infrequently-accessed pages in RAM before they reach disk
- Reduces disk write frequency — disk swap is slower and wears SSDs
- Pool size: 20% of total RAM (configurable in `/etc/luna/sysctl.toml`)

```toml
# /etc/luna/sysctl.toml
[vm]
swappiness           = 10    # Kernel aggressively uses RAM before swapping
vfs_cache_pressure   = 50    # Balance between page cache and directory/inode cache
zswap.enabled        = 1
zswap.max_pool_percent = 20
```

**Tier 2 — Disk swap partition or swapfile:**

```
TODO:
Decision not yet finalized.
Reason: Whether v1 uses a swap partition or a swapfile is undecided.
Swap partition: Fixed size, faster, simpler setup.
Swapfile: More flexible, user can resize without repartitioning.
Recommendation: Swapfile at /swapfile, sized to 4 GB by default.
Must be recorded as a Decision Log entry before installer work begins.
```

### Memory Pressure Handling

Under memory pressure, the kernel reclaims memory in this order:

1. Page cache (filesystem I/O cache) — reclaimed first, can be re-read from disk
2. Anonymous pages pushed through zswap (compressed in RAM)
3. Compressed pages evicted from zswap to disk swap
4. OOM killer invoked if all above are exhausted

The OOM killer target is configured to prefer killing processes in the luna-ai.slice before processes in the compositor or shell slices. This is configured via cgroup v2's `memory.oom.group` and individual OOM score adjustments:

| Process | OOM Score Adj | Meaning |
|---|---|---|
| `lgp-compositor` | -900 | Almost never OOM-killed |
| `luna-shell` | -800 | Almost never OOM-killed |
| `luna-ai-d` | 0 | Default — killed if memory pressure is severe |
| `ollama` | 300 | Preferred OOM target — model can be reloaded |
| User applications | 0 (default) | Killed before shell, after AI |

```
TODO:
Decision not yet finalized.
Reason: OOM score adjustments above are initial estimates.
The OOM killer is a last resort. The cgroup memory.max limits in 05_scheduler.md
should prevent the OOM killer from being needed in normal operation.
Values must be validated during stress testing.
```

---

## Technical Details

### Per-Slice Memory Limits

From `05_scheduler.md` — reproduced here for completeness:

| Slice | memory.high (soft limit) | memory.max (hard limit) |
|---|---|---|
| luna-compositor.slice | 512 MB | 1 GB |
| luna-shell.slice | 256 MB | 512 MB |
| luna-session.slice | none | none |
| luna-system.slice | 256 MB | 512 MB |
| luna-ai.slice | 5 GB | 6 GB |

When a cgroup exceeds `memory.high`, the kernel slows memory allocations for processes in that cgroup and increases reclaim pressure. This is a soft warning. When `memory.max` is reached, the OOM killer targets a process within that cgroup.

### Kernel Memory Accounting

`CONFIG_MEMCG=y` enables memory cgroup accounting. This allows:
- `luna-init-ctl` to report per-service memory usage
- cgroup memory limits to function correctly
- Per-service OOM kill targeting

Memory accounting has a small overhead (typically 1-2% CPU for allocation-heavy workloads). This overhead is acceptable.

### Shutdown Summarization

To ensure data integrity, `luna-ai-d` performs a memory summarization sweep on SIGTERM. The kernel grants `luna-init` a maximum of 5 seconds to complete this write-back before forcibly terminating the daemon.

### sysctl Memory Tuning

```toml
# /etc/luna/sysctl.toml
[vm]
swappiness           = 10     # Prefer RAM over swap until 90% full
dirty_ratio          = 10     # Max % of RAM that can be dirty pages
dirty_background_ratio = 5    # Start background writeback at 5% dirty
vfs_cache_pressure   = 50     # Moderate inode/dentry cache retention
overcommit_memory    = 1      # Allow memory overcommit (standard Linux desktop behavior)
overcommit_ratio     = 50     # When overcommit = 2, use this ratio (not applicable with =1)
```

`vm.swappiness = 10` aggressively keeps working data in RAM, swapping only as a last resort. This is correct for a desktop AI workload where Ollama model weights must stay in RAM for fast inference.

---

## LUNA.AI Memory Subsystem

This section documents the `~/.luna/memory/` data store — LUNA.AI's persistent memory of user patterns and preferences. This is not OS memory. It is a structured filesystem-based database.

### Directory Structure

```
~/.luna/
├── memory/
│   ├── patterns/
│   │   ├── workflow.db          # SQLite: workflow pattern records
│   │   └── app_sequences.db     # SQLite: application launch sequences
│   ├── context/
│   │   └── session_history.db   # SQLite: recent session context
│   └── preferences/
│       └── learned.toml         # TOML: inferred user preferences
├── config/
│   ├── observe.toml             # Observation allow-list (deny-by-default)
│   └── luna.toml                # LUNA behavior overrides
└── logs/
    └── luna-ai-d.log            # AI daemon log (user-readable)
```

### Privacy Guarantees (Core Law II)

The following behaviors are guaranteed by Core Law II and cannot be overridden by any other system component:

1. **Exclusive read access.** Only `luna-ai-d` reads `~/.luna/memory/`. No other process has permission to read this directory.

2. **No automatic transmission.** `luna-ai-d` never makes outbound network connections to transmit memory data.

3. **`luna memory --clear` always works.** This command:
   - Stops `luna-ai-d` observation (sends SIGUSR1 to pause)
   - Deletes all files in `~/.luna/memory/`
   - Deletes the encrypted persistent summary (DL-023)
   - Reinitializes empty database files
   - Resumes `luna-ai-d` with a clean memory state
   - This command cannot be disabled.

4. **`luna memory --audit` always works.** Produces a human-readable log of all patterns stored.

5. **Observation is deny-by-default.** New applications are never observed without an explicit entry in `~/.luna/config/observe.toml`. The allow-list is populated during installation (DL-022), never modified without user action.

### observe.toml Format

```toml
# ~/.luna/config/observe.toml
# Applications allowed for pattern observation
# Remove an entry to stop observing that application.

[[observe]]
app   = "code"              # Application binary name
scope = ["file_open", "build_run"]   # What to observe
since = "2025-01-01"        # When observation started

[[observe]]
app   = "firefox"
scope = ["tab_pattern"]
since = "2025-01-15"
```

No application appears in this file until the user explicitly adds it. `luna-ai-d` reads this file at startup and on SIGHUP. Changes take effect without restart.

### Pattern Database Schema

The `workflow.db` SQLite database stores observed patterns:

```sql
CREATE TABLE patterns (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    app         TEXT NOT NULL,          -- Application name
    pattern_type TEXT NOT NULL,         -- e.g., "build_run", "file_open"
    context     TEXT,                   -- JSON context at time of observation
    observed_at INTEGER NOT NULL,       -- Unix timestamp
    confidence  REAL DEFAULT 0.0,       -- 0.0–1.0 confidence score
    dismissed   INTEGER DEFAULT 0,      -- Times user dismissed this pattern's suggestion
    suppressed  INTEGER DEFAULT 0       -- 1 if Three-Strike Rule applied
);
```

The Three-Strike Rule (`core_laws.md` Law IV): after three dismissals (`dismissed >= 3`), `suppressed` is set to 1 and the pattern never generates a suggestion again until the user manually resets it.

Confidence threshold for suggestion generation: ≥ 0.75 (configurable, minimum floor: 0.5). See `core_laws.md` Law IV.

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| Memory pressure daemon | v1 | Userspace daemon that proactively manages memory pressure signals from kernel |
| AI memory encryption at rest | **v1** | DL-023: Encryption is a v1 requirement — not deferred |
| musl impact on memory layout | v2 | musl’s allocator behaves differently from glibc’s — reassess at migration |
| AI memory compaction | v1.5 | Periodic cleanup of low-confidence, old patterns from workflow.db |
| Swapfile auto-sizing | v1 | Installer sizes swapfile based on detected RAM |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Swap type (partition vs. swapfile).** Must be a Decision Log entry before installer work.

2. ~~**Default Ollama model eager vs. lazy loading.**~~ **Resolved (DL-021).** LLM Inference Engine is lazy-loaded. Ollama is not resident at boot.

3. **OOM score adjustment values.** Initial estimates. Must be validated under memory pressure testing.

4. **observe.toml scope values.** The allowed observation scope strings are placeholders. Full set of observable events must be defined in Volume IV / 03_context_engine.md.

5. ~~**AI memory encryption.**~~ **Resolved (DL-023).** Memory encryption at rest is a v1 requirement. Key management design is required (tied to user login credentials).

6. **Summarization timeout budget.** How long luna-ai-d’s shutdown summarization may take before luna-init proceeds regardless. Must be a Decision Log entry.

---

## AI Context

An AI agent working on LunaOS memory systems must understand:

- **There are two memory systems.** System memory (RAM, swap, kernel) and LUNA.AI memory (`~/.luna/memory/`). They are unrelated. Do not confuse them.
- `~/.luna/memory/` is owned by the user and readable only by `luna-ai-d`. No other process should open, read, or write files in this directory. This is enforced by both filesystem permissions and Core Law II.
- `luna memory --clear` is a user right (Core Law V). It must always work. No code path may disable or intercept it.
- `vm.swappiness = 10` is intentional. Do not increase it. Swapping Ollama model weights to disk destroys inference performance.
- zswap is enabled with a 20% pool. This reduces disk swap writes but does not eliminate them.
- The OOM killer is configured to prefer killing Ollama over the compositor or shell. This is intentional — losing AI inference temporarily is preferable to losing the desktop.
- `luna-ai-d` runs as the user (not root). It cannot access other users' memory stores or system-level files.
- The confidence threshold for suggestions is 0.75 (configurable floor: 0.5). Generating a suggestion below threshold violates Core Law IV.
- All memory limits in cgroup slices are documented in `05_scheduler.md`. Do not change them without updating that document.

---

*Document: `Volume II / 06_memory.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.2-draft*
*Depends on: architecture_overview.md, linux_architecture.md, scheduler.md, core_laws.md (Law II, IV, V), non_negotiables.md, decision_log.md (DL-008, DL-021, DL-022, DL-023)*
*Supersedes: v0.1-draft (Ollama assumed boot-resident; memory encryption incorrectly deferred to v2)*
