# LGP Bring-up Sequence
**Mahina OS — UEFI to Desktop, with exact ownership-transfer points**
**Companion to:** `LGP_IMPLEMENTATION_PLAN.md`, `LGP_RISK_ASSESSMENT.md`

This document answers the six specific questions Phase 4 asks, against the actual current `luna-init`/`luna-splash` implementation (not an idealized version of it), and proposes the minimal new code required to close the gap between "boot ends at an interactive shell" (today) and "boot ends at a working compositor" (target).

---

## 1. The full chain, as it exists and as it will exist

```
UEFI
  │  Firmware POST, ESP located
  ▼
Limine                                          [boot/limine.conf — unchanged by this work]
  │  1s menu timeout, loads vmlinuz-mahina + initramfs-mahina.img
  ▼
Linux Kernel                                    [kernel/.config — unchanged by this work]
  │  Hardware detection, DRM_KMS_HELPER + DRM_FBDEV_EMULATION both compiled in
  │  fbdev emulation claims the primary display as /dev/fb0 (implicit DRM master holder)
  ▼
Initramfs
  │  luna-init (static binary) executed as PID 1
  │  Real root mounted, re-exec from real root (Volume II / 02_boot_flow.md Stage 1)
  ▼
luna-init  [Stages 0–4 — EXISTING CODE, unchanged by this work]
  │  Stage 0: PID 1 alive, log init
  │  Stage 1: epoll/signalfd setup  →  splash_start() fork()s luna-splash HERE (existing)
  │  Stage 2: mount_fstab()
  │  Stage 3: hostname, entropy
  │  Stage 4: service_load_all() + depgraph + supervisor_start_all()
  │           (lgp-compositor.toml is a Stage-4-shaped service file, but see §3 — it must
  │            NOT actually be allowed to race ahead of splash teardown, so it is NOT
  │            started by the generic supervisor_start_all() batch — see §3)
  ▼
luna-splash  [unchanged by this work]
  │  Opens /dev/fb0, mmaps, renders logo + progress text + bar
  │  Receives "PERCENT|MSG\n" over pipe from luna-init
  ▼
═══════════════════ NEW CODE BEGINS HERE — Stage 5 ═══════════════════
  │  (see §3 for exact main.c insertion point and pseudocode)
  ▼
LGP Compositor
  │  M1: open /dev/dri/card0 → enumerate connectors → atomic mode set → dumb buffer → page flip
  │  M2: bind/listen /run/lgp/compositor.sock
  │  M3: ready to accept LGP_HELLO handshakes
  │  (DL-031 D-Bus Ready signal — separate concern, see §4)
  ▼
LunaGUI                                         [M4+ — not built by this plan]
  ▼
Desktop                                         [Stage 6/7 — not built by this plan]
```

---

## 2. Framebuffer ownership transfer — exact point

**Today:** `luna-splash` owns `/dev/fb0` from the moment `splash_start()` forks it (inside `main.c`'s `STAGE 1` block) until `splash_stop()` is called. Nothing else touches display hardware.

**Transfer point (new):** ownership transfers from `luna-splash` to `lgp-compositor` at the instant `splash_stop()`'s internal `waitpid(splash_pid, NULL, 0)` returns — confirmed by reading `src/luna-init/splash.c`, this call is already synchronous in the current code (it blocks until the child has actually exited, not merely been signaled). This is the correct and sufficient signal: when the fbdev character device's last open file descriptor closes (which happens as part of normal process teardown when `luna-splash` exits, whether or not its `render.c` explicitly calls `close()` on `/dev/fb0` before exiting), the kernel's fbdev-over-DRM helper releases the implicit DRM master grab it held on behalf of that client. Only after this does `open("/dev/dri/card0", O_RDWR)` followed by an atomic mode-set from a *new* process reliably succeed without `EBUSY`/`EACCES`.

**Why this matters for sequencing, concretely:** if `lgp-compositor` were started in parallel with `luna-splash` still running (e.g., as part of the generic Stage 4 `supervisor_start_all()` batch, which has no ordering relationship to the splash teardown because `luna-splash` isn't a TOML-tracked service), the compositor's `drmSetMaster()`-equivalent step would race the fbdev helper's master grab and could fail nondeterministically depending on scheduling — exactly the kind of bug that passes in development and fails intermittently on real hardware. This is why §3 below does not route `lgp-compositor` through the ordinary Stage 4 service batch.

---

## 3. DRM master acquisition, KMS modesetting, and the new Stage 5 block

`src/luna-init/main.c` currently has no code between the end of Stage 4 (`supervisor_is_boot_complete()` returning true) and dropping to an interactive shell. The minimal new code required is a Stage 5 block inserted at that exact point, before the existing shell-fork logic:

```c
/* ═══ STAGE 5 (NEW): Graphics Layer ═══════════════════════════════════ */

luna_log_set_stage(LUNA_STAGE_GRAPHICS);          /* new stage constant */
LUNA_INFO("luna-init", "Stage 5: Releasing boot splash");

splash_stop();   /* EXISTING function — already synchronous waitpid().
                   * This is the framebuffer ownership transfer point (§2). */

LUNA_INFO("luna-init", "Stage 5: Starting lgp-compositor");
int comp_result = supervisor_start_one("lgp-compositor");   /* EXISTING primitive,
                                                               * already used by
                                                               * supervisor_start_all()
                                                               * internally — not new
                                                               * architecture, just a
                                                               * new call site. */
if (comp_result < 0) {
    LUNA_WARN("luna-init", "lgp-compositor failed to start — degraded graphics mode");
    /* Per 02_boot_flow.md Stage 5 row: "Fallback to framebuffer console".
     * No new fallback code is required here: the existing supervisor restart
     * policy (policy=always, attempts=3, delay_ms=1000, already configured in
     * lgp-compositor.toml) takes over exactly as it would for any other
     * service. luna-init simply proceeds to Stage 6 logic in a degraded state. */
}
/* The existing READY_SOCKET poll against /run/lgp/compositor.sock (already
 * configured, timeout_ms=5000) is luna-init's own readiness gate — see §4.
 * This blocks (within the supervisor's existing async pump, not a hard
 * blocking call — supervisor_pump() already drains this on the timerfd) until
 * RUNNING or DEGRADED. */
```

This requires exactly two things not present today: (1) a `LUNA_STAGE_GRAPHICS` constant added to the existing `log.h` stage enum (purely additive — does not touch the "Things AI must never change" boot-stage-sequence rule, since it adds a stage rather than altering 0–4), and (2) `lgp-compositor` being **excluded from the generic Stage 4 `supervisor_start_all()` batch** and instead started explicitly at this Stage 5 call site. The cleanest way to do the exclusion without inventing a new TOML field is to keep `lgp-compositor.toml` exactly as committed but have `service_load_all()`/`depgraph_build()` skip starting it automatically (e.g., a `start = "manual"` style flag, or simply special-casing the name "lgp-compositor" in `supervisor_start_all()` the same way Stage 4 already special-cases Ollama's absence per DL-021). This is a small, additive change to existing logic, not new architecture — flag it for review as the one piece of this bring-up plan that touches already-shipped Stage 0 code rather than purely adding new Stage 5+ code.

**DRM master acquisition** happens inside the compositor's own M1 startup path (`drm/device.c`'s `open()` call), immediately upon `lgp-compositor` being `execve()`'d by the Stage 5 block above. There is no separate "acquire master" step exposed by modern DRM for the single-display-server case — opening the device as the first (and, per `01_lgp.md`/`03_compositor.md`, *only*) client is sufficient, which is exactly why §2's ordering guarantee (splash fully exited first) is the load-bearing constraint, not an explicit `drmSetMaster()` retry loop.

**KMS modesetting** begins immediately after device open, inside the same M1 path: connector enumeration → mode selection → atomic commit, per `02_rendering_pipeline.md`'s documented sequence, before any GPU backend initialization (Stage 2 has no GPU backend at all — see DL-026) and before the LGP socket is created. This ordering (DRM/KMS configured *before* the socket exists) is the same ordering `03_compositor.md`'s eight-step startup sequence specifies and matches M1-before-M2 in the implementation plan.

---

## 4. Compositor readiness — how `luna-init` knows, vs. how other consumers know

Two genuinely different audiences need "is the compositor ready" information, and this plan deliberately answers them two different ways rather than forcing DL-031's single D-Bus mechanism onto both (see `LGP_IMPLEMENTATION_PLAN.md` §2.2-C for the full reasoning):

- **`luna-init` itself** (the Stage 5 block above): uses the **already-implemented, already-configured** `READY_SOCKET` poll against `/run/lgp/compositor.sock`, with the `timeout_ms = 5000` already present in the committed `lgp-compositor.toml`. This requires zero new luna-init capability — `wait_for_ready()`'s `READY_SOCKET` case already exists and is exercised by other services today.
- **Stage 6+ consumers with their own D-Bus client** (`luna-shell`, `luna-island` — neither exists yet, out of scope for this plan): continue to wait on `org.mahina.compositor.Ready` exactly as DL-031 specifies, emitted by the compositor itself once it has a D-Bus connection (a capability the compositor can have even though `luna-init` cannot, since the compositor is not required to be statically linked the way PID 1 is).

This split should be ratified as a Decision Log amendment before Task 3 lands (proposed `DL-054` in the implementation plan) so it is recorded rather than inferred from the gap between DL-031's text and the committed TOML file.

---

## 5. Splash release — exact mechanics (already correct, needs only a new call site)

`splash_stop()` in the current `src/luna-init/splash.c`:
1. Closes the write end of the IPC pipe.
2. Sends `SIGTERM` to `luna-splash`.
3. Calls `waitpid(splash_pid, NULL, 0)` — **synchronous**, blocks until the child has fully exited.
4. Sets `splash_pid = -1`.

This already matches DL-043's requirement and `02_boot_flow.md`'s documented Stage 5 handoff ("luna-init sends SIGTERM to luna-splash before the compositor takes over"). No change to `splash.c` is required — only the call site moves from "right before the dev shell fork" to "right at the start of the new Stage 5 block," per §3 above.

---

## 6. First compositor frame and the accepted black-frame cut

Per DL-043, Mahina accepts a single black frame (~16ms at 60Hz) between the last `luna-splash` frame and the compositor's first frame, and explicitly declines to add architectural complexity to eliminate it. This plan does not attempt to eliminate it either — M1's first successful page flip (a solid `LUNA Void`-colored dumb buffer, pending the Color Semantic Contract resolution in `LGP_PROTOCOL_REVIEW.md` for the exact hex value) *is* "the compositor's first frame" in DL-043's terms, and the gap between splash's last frame and that flip is exactly the accepted cut — nothing more needs to be engineered here.

---

## 7. Failure fallback — what happens when graphics bring-up fails

`02_boot_flow.md`'s Stage 5 row specifies: entry condition "GPU/framebuffer ready," success criterion "LGP compositor accepting connections," failure behavior "Fallback to framebuffer console." Concretely, with the Stage 5 block in §3:

- **DRM open/modeset failure inside the compositor (M1):** the compositor process logs `FATAL` ("DRM device lost" — already in `03_compositor.md`'s log table) and exits non-zero. `luna-init`'s existing supervisor restart policy (`policy=always`, `attempts=3`, `delay_ms=1000`) retries up to 3 times. This is unmodified existing supervisor behavior — no new failure-handling code needed.
- **Socket never appears within `timeout_ms=5000` (M2 not reached):** `READY_SOCKET` polling times out, the service is marked per the existing supervisor state machine (same DEGRADED path any other service takes after exhausting restart attempts).
- **Compositor enters `DEGRADED`:** the Stage 5 block (§3) observes this via the same supervisor state query `luna-init-ctl status` already exposes, logs `WARN`, and — per `02_boot_flow.md`'s documented "degraded desktop" behavior — proceeds to whatever Stage 6 logic exists (none yet) in a degraded state rather than retrying indefinitely. Until Stage 6 exists, the practical effect for this plan's scope is: fall through to the existing interactive-shell drop, exactly as boot does today when there is no compositor at all. This is not a regression — it is the same fallback Mahina already has, now reached deliberately rather than because Stage 5 doesn't exist.
- **Mid-session crash after the compositor was previously healthy:** owned by `luna-init` per `13_component_ownership.md`'s Failure Mode Ownership table, using the exact restart-then-degrade sequence `03_compositor.md` §Crash Recovery already specifies (detect via `waitpid`/SIGCHLD → clean up the stale socket file → restart → re-signal readiness; second crash within 30 seconds → degraded, no further restarts). This sequence is fully specified already and requires no new design from this plan beyond the socket-cleanup implementation already called out as part of M2.

---

## 8. Summary table

| Question | Answer |
|---|---|
| Framebuffer ownership transfer | At `splash_stop()`'s `waitpid()` return, inside the new Stage 5 block |
| DRM master acquisition | Implicit, on `open(/dev/dri/card0)` inside M1, immediately after the Stage 5 block starts the compositor |
| KMS modesetting begins | Immediately after device open, before GPU backend init (none in Stage 2) and before socket creation |
| Splash released | `SIGTERM` + synchronous `waitpid()` — already implemented in `splash.c`; only the call site changes |
| Compositor starts drawing | First successful page flip in M1 — this is also DL-043's accepted single black-frame cut |
| Failure fallback | Existing supervisor restart/degrade policy (already configured in `lgp-compositor.toml`) + fall-through to the existing interactive-shell path, matching `02_boot_flow.md`'s documented "framebuffer console" fallback |
