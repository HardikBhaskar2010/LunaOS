# Mahina — Updater
**Volume V · Chapter 7**
**Classification:** Core Architecture — Userland
**Status:** Canonical · Specifies the Mahina rolling release update system

---

## Purpose

This document specifies **luna-update** — the Mahina update daemon and the rolling release update pipeline. Mahina uses a rolling release model (DL-001): there are no major version upgrades. The system updates continuously, delivering individual package updates as they are available.

This document defines:
- How updates are discovered
- How they are applied safely
- How LUNA communicates update availability
- How the user controls the update process
- Rollback via Btrfs snapshots

---

## Overview

```
Update pipeline:

  Repository servers (packages.lunaos.dev)
       │  (luna-update polls every 6 hours by default)
       ▼
  luna-update daemon
  (checks for newer versions of installed packages)
       │
       │  Updates available?
       ▼
  Notify LUNA (org.lunaos.luna.SendObservation)
  LUNA notifies user via luna-island (ambient, non-intrusive)
       │
       │  User acknowledges
       ▼
  Download packages (background, in cache)
       │
       ▼
  User initiates apply (or auto-apply if configured)
       │
       │  Pre-update:
       ▼
  Btrfs snapshot: "lpkg-pre-update-<timestamp>"
       │
       ▼
  Apply updates (atomic, per-package transactions)
       │
       ▼
  Post-update verification
       │
       ├── Kernel update? → notify: "Reboot required for kernel update"
       └── Application update? → notify: "Restart <app> to complete update"
```

---

## luna-update Daemon

### Process Model

```
luna-update daemon (system service — started by luna-init):

  Not a user process — runs as a low-privilege system user (luna-update).
  Communicates with lpkg D-Bus service for package operations.
  Communicates with luna-ai-d for update notifications.
  Writes to: /var/cache/lpkg/ (package downloads)
             /var/log/luna-update.log (audit log)
```

### Update Check Cycle

```c
void update_check_loop() {
    while (running) {
        // 1. Refresh repository metadata
        lpkg_update_repo_metadata();

        // 2. Check for upgradeable packages
        pkg_list_t *upgrades = lpkg_get_upgrades();

        if (upgrades->count > 0) {
            // 3. Pre-download in background (optional, configurable)
            if (config.predownload_updates)
                lpkg_download_packages(upgrades);

            // 4. Notify LUNA
            notify_luna_updates_available(upgrades);
        }

        // 5. Sleep until next check
        sleep(config.check_interval_hours * 3600);
    }
}
```

### Update Notification to LUNA

```python
# luna-update → luna-ai-d via D-Bus

def notify_luna_updates_available(updates: list) -> None:
    """
    Notify LUNA that updates are available.
    LUNA's Personality Engine decides when and how to tell the user
    based on current mode and confidence.
    """
    luna.SendObservation(
        type = "UPDATES_AVAILABLE",
        data = {
            "count":          len(updates),
            "includes_kernel": any(u.name == "linux" for u in updates),
            "total_size_mb":  sum(u.download_size_mb for u in updates),
            "update_list":    [u.name for u in updates[:5]]  # first 5 names
        }
    )
```

LUNA's template response:
```
"3 updates available (linux, glibc, luna-shell). ~42 MB. Apply now?"
"Kernel update available. A reboot will be required."
```

---

## Rolling Release Safety Model

### Why Rolling Is Safe in Mahina

Rolling release on other distros can break systems because updates are applied without safeguards. Mahina rolling release is safe because of:

```
Rolling release safety stack:

  1. Pre-update Btrfs snapshot (lpkg)
     → If something breaks: rollback to snapshot in < 3 seconds

  2. Atomic per-package transactions (DL-018)
     → Failed update = automatically rolled back per package
     → System never left partially updated

  3. Repository signing (DL-019)
     → Only verified, signed packages are applied

  4. Staged rollout
     → Large updates (kernel, glibc) staged to -testing first
     → Applied to stable repo only after testing validation period

  5. User controls timing
     → Updates are never applied automatically without user knowledge
     → Auto-apply is opt-in (configurable, defaults to notify-only)
```

### Update Priority Classes

```
Update priority classes:

  SECURITY:   Apply immediately. LUNA notifies urgently.
              Example: CVE patch for kernel, glibc, openssl.
              Behavior: LUNA notifies immediately regardless of mode.
                        User can apply with one click.

  NORMAL:     Apply when convenient.
              Example: Package version bumps, feature updates.
              Behavior: LUNA notifies in AMBIENT mode.
                        User applies manually or via auto-apply.

  OPTIONAL:   Not applied automatically.
              Example: Optional language packs, alternative themes.
              Behavior: Visible in luna-settings software list.
                        User must explicitly install.
```

---

## Update Application Flow

### Manual Update (user-initiated)

```bash
# Via CLI
lpkg update       # refresh metadata
lpkg upgrade      # apply all NORMAL + SECURITY updates

# Via luna-settings
Settings → Software → Updates → Apply Updates
```

### Auto-Apply Mode (opt-in)

```toml
# ~/.luna/config/luna.toml
[updates]
auto_apply       = false           # off by default
auto_apply_scope = "security"      # "security" | "security+normal"
apply_time       = "03:00"         # apply at 3am when enabled
notify_before    = true            # notify before auto-applying
notify_before_min = 10             # 10 minutes advance notice
```

When auto-apply is enabled:
```
Auto-apply sequence (03:00):

  1. luna-update checks: is user active? (Presence Engine)
  2. If user is active (not GAMING/FOCUS): notify + wait for confirmation
  3. If user is idle (IDLE mode): apply silently
  4. Take Btrfs snapshot
  5. Apply updates
  6. If kernel updated: notify "Reboot when convenient"
  7. Log to /var/log/luna-update.log
```

---

## Kernel Update Handling

Kernel updates require special handling because:
- The running kernel cannot be replaced mid-session
- A reboot is required to boot into the new kernel

```
Kernel update flow:

  1. New kernel package available
  2. luna-update downloads kernel + initramfs
  3. LUNA notifies: "Kernel update available. A reboot is required after applying."
  4. User confirms
  5. lpkg installs new kernel to /boot/ (does NOT remove old kernel)
  6. limine config updated to default to new kernel
     (old kernel entry kept as fallback for 2 boot cycles)
  7. LUNA notifies: "Kernel installed. Reboot when ready."
  8. User reboots (via luna-settings or `luna shutdown --reboot`)
  9. On boot: new kernel active
  10. After successful boot: old kernel entry can be pruned (after 2 clean boots)
```

### Kernel Rollback

If the new kernel fails to boot (kernel panic, driver failure):
```
limine fallback:
  Old kernel entry is still in limine config.
  User boots into old kernel from limine boot menu.
  On login: LUNA detects fallback boot → notifies user.
  User runs: lpkg rollback linux
    → Restores previous kernel as default
    → Removes failed kernel package
```

---

## Update Audit Log

```
/var/log/luna-update.log format:

2026-06-27 03:00:01  CHECK     repository metadata refreshed, 3 updates found
2026-06-27 03:00:02  SNAPSHOT  lpkg-pre-update-1719459602 created
2026-06-27 03:00:05  UPDATE    luna-shell 1.0.0 → 1.0.1   SUCCESS
2026-06-27 03:00:08  UPDATE    lgp-compositor 1.0.0 → 1.0.1  SUCCESS
2026-06-27 03:00:15  UPDATE    linux 6.6.32 → 6.6.33   SUCCESS (reboot required)
2026-06-27 03:00:15  COMPLETE  3 updates applied. Reboot required for kernel.
```

Users can view update history via:
```bash
luna update log          # recent updates
luna update log --full   # full history
```

---

## Update Rollback

If an update causes problems, the user can roll back:

```bash
luna rollback list
  → Lists available pre-update snapshots:
    "lpkg-pre-update-2026-06-27-03:00  (3 updates applied)"
    "lpkg-pre-update-2026-06-25-14:30  (1 update applied)"

luna rollback apply lpkg-pre-update-2026-06-27-03:00
  → Confirms: "This will restore your system to its state before 3 updates.
               Your home directory is not affected. Continue? [y/N]"
  → On confirm: Btrfs restore from snapshot
  → Reboot required for kernel rollback

luna rollback auto
  → Rolls back to the most recent pre-update snapshot
```

D-Bus equivalent: `org.lunaos.pkg.RollbackToSnapshot(snapshot_id: string)`

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Rolling release model | DL-001 | ✅ Accepted |
| Pre-update Btrfs snapshot | DL-027 | ✅ Accepted |
| Atomic per-package transactions | DL-018 | ✅ Accepted |
| Auto-apply defaults to off | This document | ✅ Accepted |
| Security updates notified urgently | This document | ✅ Accepted |
| Old kernel kept for 2 boot cycles after update | This document | ✅ Accepted |
| Check interval: 6 hours default | This document | 🧪 Experimental |
| luna-update runs as system user, not root | This document | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Delta updates.** Instead of downloading full packages on update, delta updates download only the changed bytes. Significantly reduces bandwidth. Tools: bsdiff/bspatch. Must be a Decision Log entry — adds implementation complexity but strong user benefit.

2. **Update staging.** The spec mentions a -testing repository for staged rollouts. The staging pipeline (how a package moves from testing to stable, what validation is done) must be documented in Volume VI/07 (Release Process).

3. **Post-update app restart.** When luna-shell or luna-ai-d updates, the user needs to restart them. How does Mahina notify and facilitate this? Session restart? Per-process restart? Must be specified.

4. **Snapshot pruning.** Pre-update snapshots from > 7 days ago are pruned. But if the user never reboots (long uptime), many snapshots accumulate. A maximum snapshot count (regardless of age) should also apply. Must be specified in lpkg.

5. **Metered connections.** On a metered network (mobile data), pre-downloading updates silently would be costly. `luna-update` should respect the network metered flag (from luna-netd) and not pre-download on metered connections.

---

## AI Context

- luna-update is a **system daemon**, not a user process. It runs as `luna-update` user, not as the logged-in user. It does not have access to the user's home directory or files. It communicates via D-Bus only.
- The Btrfs snapshot must be taken before EVERY update transaction, even small single-package updates. The snapshot is what guarantees safety. Never skip it for "small" updates.
- LUNA communicates update availability — she does not apply updates autonomously. The user must confirm update application every time unless auto-apply is explicitly enabled. This is Core Law V.
- Kernel updates require a reboot. This is non-negotiable. Never claim a kernel update is "applied" without a reboot. If a reboot is needed, LUNA's notification must make this clear.
- The rolling release model means there is no "Mahina 2.0 upgrade" that users dread. Updates are continuous and small. The safety model (Btrfs + atomic transactions + rollback) makes this viable.

---

*Document: `Volume V / 07_updater.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume V/03_package_manager.md, Volume II/09_filesystem.md, DL-001, DL-018, DL-027*
*Informs: Volume VI/07_release_process.md*
