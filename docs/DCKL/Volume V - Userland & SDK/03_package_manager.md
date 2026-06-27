# Mahina — Package Manager (lpkg)
**Volume V · Chapter 3**
**Classification:** Core Architecture — Userland
**Status:** Canonical · This document is the authoritative specification for lpkg, the Mahina package manager

---

## Purpose

This document specifies **lpkg** — the Mahina package manager. lpkg is the system that installs, updates, removes, and manages software on Mahina. It is custom-built (DL-003) because no existing package manager satisfies Mahina's requirements for atomic transactions, Btrfs snapshot integration, per-user installation, and LUNA-integrated privilege escalation.

lpkg is not apt. It is not pacman. It is purpose-built for Mahina's architecture and its values: the user owns the machine, installations are safe and reversible, and third-party software is verified but never blocked.

---

## Overview

```
lpkg components:

  ┌────────────────────────────────────────────────────────────┐
  │                         lpkg                               │
  │                                                             │
  │  ┌──────────────┐  ┌───────────────┐  ┌────────────────┐  │
  │  │  CLI Frontend │  │  D-Bus Service│  │  LunaGUI Dialog│  │
  │  │  `lpkg ...`  │  │ org.mahina.  │  │  (graphical    │  │
  │  │              │  │  pkg`        │  │   confirmations│  │
  │  └──────┬───────┘  └───────┬───────┘  └───────┬────────┘  │
  │         └──────────────────┴──────────────────┘           │
  │                            │                               │
  │                    ┌───────▼──────────────┐                │
  │                    │   Package Engine      │                │
  │                    │  (core logic)         │                │
  │                    │  Resolution           │                │
  │                    │  Download             │                │
  │                    │  Verification         │                │
  │                    │  Transaction          │                │
  │                    │  Rollback             │                │
  │                    └───────┬──────────────┘                │
  │              ┌─────────────┼────────────────┐              │
  │              ▼             ▼                ▼              │
  │       Repository DB    Package DB      Snapshot            │
  │       (remote pkg info)(installed)     (pre-install        │
  │                                         Btrfs snap)        │
  └────────────────────────────────────────────────────────────┘
```

---

## Package Format: LPKG

An lpkg package is a compressed archive with a specific structure:

```
Package file: <name>-<version>-<arch>.lpkg  (e.g. firefox-120.0-x86_64.lpkg)

Archive format: tar.zst (zstandard compression)

Contents:
  luna.toml        — package manifest (required)
  files/           — all installed files (mirroring target filesystem layout)
    usr/
      bin/
      lib/
      share/
  hooks/           — optional lifecycle scripts
    pre-install.sh
    post-install.sh
    pre-remove.sh
    post-remove.sh
  observe.toml     — LUNA observation rules for this app (optional)
  apparmor.d/      — AppArmor profiles for this app (optional)
```

### luna.toml Manifest

```toml
# Package manifest — luna.toml (inside the .lpkg archive)

[package]
name          = "firefox"
version       = "120.0"
release       = 1                   # increment for same-version rebuilds
arch          = "x86_64"            # "x86_64" | "aarch64" | "any"
description   = "Fast, private browser"
homepage      = "https://mozilla.org"
license       = "MPL-2.0"

[package.author]
name          = "Mozilla Foundation"
email         = "packaging@mahina.dev"  # the package maintainer

[package.install]
scope         = "user"              # "user" | "system" (DL-017)
prefix_user   = "~/.local"         # per-user install prefix
prefix_system = "/usr"             # system-wide prefix

[package.dependencies]
required      = ["glibc>=2.35", "libpng>=1.6"]
optional      = ["ffmpeg"]
conflicts     = ["chromium<90"]

[package.capabilities]
# Privileges this app needs (checked by compositor + AppArmor)
# Values: "top_layer", "canvas", "clipboard_read", "clipboard_write"
requires      = ["canvas"]

[package.app]
# Information for the application launcher
name          = "Firefox"
icon          = "files/usr/share/icons/hicolor/256x256/apps/firefox.png"
categories    = ["Browser", "Internet"]
exec          = "firefox %u"       # %u = URI argument
keywords      = ["browser", "web", "internet"]
```

---

## Database Schema

### Installed Package Database

```
Location:
  Per-user: ~/.local/share/lpkg/installed.db  (DL-017)
  System:   /var/lib/lpkg/installed.db

Format: SQLite
```

```sql
CREATE TABLE packages (
    pkg_id        TEXT PRIMARY KEY,     -- "name-version-release-arch"
    name          TEXT NOT NULL,
    version       TEXT NOT NULL,
    release       INTEGER NOT NULL,
    arch          TEXT NOT NULL,
    scope         TEXT NOT NULL,        -- "user" | "system"
    install_time  INTEGER NOT NULL,     -- Unix timestamp
    install_reason TEXT NOT NULL,       -- "explicit" | "dependency"
    files_hash    TEXT NOT NULL,        -- SHA-256 of files/ directory tree
    manifest_json TEXT NOT NULL         -- full luna.toml as JSON (for queries)
);

CREATE TABLE installed_files (
    file_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    pkg_id        TEXT NOT NULL REFERENCES packages(pkg_id),
    path          TEXT NOT NULL,        -- absolute install path
    sha256        TEXT NOT NULL,        -- hash of this file
    size_bytes    INTEGER NOT NULL,
    is_config     BOOLEAN NOT NULL      -- config files are not overwritten on update
);

CREATE TABLE transactions (
    tx_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    type          TEXT NOT NULL,        -- "install" | "remove" | "update"
    pkg_id        TEXT NOT NULL,
    timestamp     INTEGER NOT NULL,
    result        TEXT NOT NULL,        -- "success" | "failed" | "rolled_back"
    snapshot_id   TEXT,                 -- Btrfs snapshot taken before this tx
    error_msg     TEXT                  -- populated on failure
);
```

---

## Atomic Transaction Model (DL-018)

Every package operation is an atomic transaction. This means the system is never left in a partially-installed state.

### Transaction Flow

```
Installation transaction:

  1. PLAN: Resolve dependencies, check conflicts, calculate disk space
  2. DOWNLOAD: Fetch package archive(s) to ~/.cache/lpkg/
  3. VERIFY: Check package signature and file hashes
  4. SNAPSHOT: Create Btrfs pre-install snapshot (DL-027)
                "lpkg-pre-install-firefox-120.0-<timestamp>"
  5. STAGE: Extract files to a staging area (not the final location yet)
  6. PERMISSION: Request privilege escalation if system install (DL-016)
  7. COMMIT: Move staged files to final location (atomic rename where possible)
              Write to installed.db
              Run post-install hooks
  8. VERIFY: Spot-check installed files against manifest hashes
  9. DONE: Record successful transaction

  On failure at any step after SNAPSHOT:
    → Rollback: restore from Btrfs snapshot (or reverse file operations)
    → Record transaction as "rolled_back"
    → Report failure to user with specific step that failed
```

### Rollback Mechanism (DL-018)

```c
typedef struct lpkg_transaction {
    char     tx_id[64];
    char     pkg_id[256];
    char     snapshot_id[256];  // Btrfs snapshot name (may be empty)
    lpkg_file_op_t *ops;        // ordered list of file operations performed
    int      ops_count;
    bool     committed;
} lpkg_transaction_t;

typedef struct lpkg_file_op {
    enum { FILE_CREATE, FILE_OVERWRITE, FILE_DELETE, FILE_MOVE } type;
    char src_path[512];
    char dst_path[512];
    char backup_path[512];      // where original was saved before overwrite
} lpkg_file_op_t;

// Rollback: replay ops in reverse
void lpkg_rollback(lpkg_transaction_t *tx) {
    if (tx->snapshot_id[0] != '\0') {
        // Btrfs snapshot available — use it (faster, more reliable)
        btrfs_restore_snapshot(tx->snapshot_id);
        return;
    }
    // No snapshot — replay file ops in reverse
    for (int i = tx->ops_count - 1; i >= 0; i--) {
        lpkg_file_op_t *op = &tx->ops[i];
        switch (op->type) {
            case FILE_CREATE:   unlink(op->dst_path); break;
            case FILE_OVERWRITE: rename(op->backup_path, op->dst_path); break;
            case FILE_DELETE:   rename(op->backup_path, op->dst_path); break;
            case FILE_MOVE:     rename(op->dst_path, op->src_path); break;
        }
    }
}
```

---

## Btrfs Snapshot Integration (DL-027)

Before every package operation that modifies system files, lpkg creates a Btrfs snapshot:

```bash
# Snapshot naming convention
lpkg-pre-install-{name}-{version}-{unix_timestamp}
lpkg-pre-update-{name}-{old_version}-{unix_timestamp}
lpkg-pre-remove-{name}-{version}-{unix_timestamp}

# Created via btrfs subvolume snapshot
btrfs subvolume snapshot / /snapshots/lpkg-pre-install-firefox-120.0-1719472800
```

**Snapshot retention:**
- Snapshots are kept for **7 days** by default (configurable)
- The last 5 snapshots are always kept regardless of age
- Users may list and restore snapshots via `lpkg snapshot list` / `lpkg snapshot restore <id>`

---

## Repository System (DL-019)

### Repository Types

```toml
# ~/.luna/config/repos.toml — repository configuration

[[repo]]
name     = "luna-official"
url      = "https://packages.mahina.dev/official"
priority = 100                   # highest priority
signed   = true
key_id   = "0xABCD1234EFGH5678"  # GPG key fingerprint

[[repo]]
name     = "luna-community"
url      = "https://packages.mahina.dev/community"
priority = 50
signed   = true
key_id   = "0xCOMMUNITYKEY"

[[repo]]
name     = "my-company-internal"
url      = "https://pkgs.mycompany.com/luna"
priority = 75
signed   = true
key_id   = "0xCOMPANYKEY"
trusted  = true                  # user has explicitly trusted this repo
```

### Package Signing & Manifest Signatures (DL-019)

```
Package signing requirement:

  Official repo:    Signed with Mahina project key (mandatory)
  Community repo:   Signed with maintainer key (mandatory)
  Third-party repo: Signed with repo owner key (mandatory)
  
  Unsigned packages: REJECTED by default
  Override: lpkg install --allow-unsigned <package> (explicit user decision)
            Shows warning dialog before proceeding

  Signature format: Ed25519 (GPG is explicitly rejected for complexity)

Manifest Signatures:
  lpkg maintains a signed manifest of allowed system assets (like AI models) 
  in /etc/luna/models.toml. First-party repositories provide a detached Ed25519 
  signature for these manifests. lpkg validates this signature using a public key 
  embedded in the base OS image.
```
  Signature file:   <package>.lpkg.sig accompanies every .lpkg file
```

---

## CLI Reference

```bash
# Installation
lpkg install <package>              # install latest version (per-user)
lpkg install --system <package>     # install system-wide (requires privilege)
lpkg install firefox==120.0         # install specific version
lpkg install ./local-app.lpkg       # install from local file

# Removal
lpkg remove <package>               # remove package (keep config files)
lpkg remove --purge <package>       # remove package + config files

# Updates
lpkg update                         # refresh repository metadata
lpkg upgrade                        # upgrade all installed packages
lpkg upgrade <package>              # upgrade specific package

# Information
lpkg search <query>                 # search available packages
lpkg info <package>                 # show package details
lpkg list                           # list installed packages
lpkg list --all                     # list all available packages
lpkg files <package>                # list files installed by package
lpkg owner <file_path>              # which package owns this file?

# Snapshot management
lpkg snapshot list                  # list lpkg snapshots
lpkg snapshot restore <id>          # restore system to snapshot state

# Maintenance
lpkg clean                          # remove cached package downloads
lpkg verify                         # verify installed package file hashes
lpkg autoremove                     # remove unused dependency packages

# Model management (AI models via lpkg — unified management)
lpkg model install llama3.1
lpkg model list
lpkg model remove phi3:mini
```

---

## Privilege Escalation (DL-016)

When a system-wide installation requires elevated privileges, lpkg triggers the LUNA graphical permission dialog:

```
Privilege escalation flow (graphical session):

  lpkg install --system firefox
       │
       ▼
  lpkg D-Bus service requests privilege from luna-ai-d:
  org.mahina.shell.RequestPrivilege(
      action = "system_package_install",
      package = "firefox 120.0",
      reason = "System-wide installation requires administrator access"
  )
       │
       ▼
  Luna Island shows permission dialog (DL-016):
  ╔══════════════════════════════════════════════════╗
  ║  ●  LUNA — System Installation                    ║
  ╠══════════════════════════════════════════════════╣
  ║  lpkg wants to install firefox 120.0             ║
  ║  system-wide. This modifies /usr.                ║
  ║                                                   ║
  ║  A Btrfs snapshot will be taken first.           ║
  ║                                                   ║
  ║          [ Allow ]           [ Deny ]            ║
  ╚══════════════════════════════════════════════════╝
       │
       ▼
  On Allow: lpkg proceeds with system installation
  On Deny:  lpkg exits with error: "User denied privilege escalation"

Terminal fallback (no graphical session):
  lpkg falls back to: sudo (or polkit) for privilege escalation
```

---

## observe.toml Installation

When an application is installed, its bundled `observe.toml` is merged into the user's observation config:

```python
def install_observe_rules(pkg_id: str, pkg_observe_toml: str):
    """
    Merge the package's observe.toml rules into the user's observe.toml.
    New rules are appended. Existing rules are never overwritten
    (user's customizations take precedence).
    """
    user_observe = load_toml("~/.luna/config/observe.toml")
    pkg_rules    = parse_toml(pkg_observe_toml)

    for rule in pkg_rules["observe"]["app_rules"]:
        if not rule_already_exists(user_observe, rule["pattern"]):
            user_observe["observe"]["app_rules"].append(rule)
            log(f"observe.toml: added rule for '{rule['pattern']}' from {pkg_id}")

    write_toml("~/.luna/config/observe.toml", user_observe)
```

---

## Performance Budget

| Metric | Target | Hard Limit |
|---|---|---|
| `lpkg search` (local cache) | < 200ms | 500ms |
| `lpkg install` metadata phase | < 2 seconds | 5 seconds |
| Package download speed | Network-limited | — |
| Btrfs snapshot creation | < 1 second | 5 seconds |
| Stage + commit (for 100MB package) | < 5 seconds | 15 seconds |
| `lpkg list` (1000 packages) | < 100ms | 300ms |
| Rollback via Btrfs snapshot | < 3 seconds | 10 seconds |

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| lpkg is a custom package manager | DL-003 | ✅ Accepted |
| Per-user install by default | DL-017 | ✅ Accepted |
| Atomic transactions with rollback | DL-018 | ✅ Accepted |
| Btrfs pre-operation snapshots | DL-027 | ✅ Accepted |
| Verification-based repo policy | DL-019 | ✅ Accepted |
| Graphical privilege escalation | DL-016 | ✅ Accepted |
| Package format: tar.zst + luna.toml | This document | ✅ Accepted |
| Package signing: ed25519 or GPG | This document | 🔵 Draft |
| Snapshot retention: 7 days / last 5 | This document | 🧪 Experimental |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Package signing algorithm.** ed25519 or GPG? GPG is more widely understood; ed25519 is more modern and simpler. Must be a Decision Log entry.

2. **LBUILD (package build system).** lpkg installs packages. LBUILD builds them. LBUILD is mentioned in earlier documents but not yet specified. Must have its own document before the repository is operational.

3. **Disk space check before install.** The spec mentions a disk space check in the PLAN phase. The exact threshold for "not enough space" (package size + snapshot overhead + staging space) must be calculated and implemented.

4. **Dependency resolution algorithm.** The spec mentions dependency resolution but does not specify the algorithm. SAT-solver based (like apt's) or simpler greedy resolution (like pacman's)? Must be decided before lpkg v1 implementation.

5. **Flat-pak / AppImage support.** Should lpkg also manage Flatpak or AppImage packages? These formats provide their own sandboxing and are common for distributing third-party Linux software. Must be a Decision Log entry.

---

## AI Context

- lpkg is the **gatekeeper for what runs on Mahina**. Every application that gets installed goes through lpkg. The verification system (signatures, hashes) is what prevents malicious software from being silently installed.
- The Btrfs snapshot must be taken **before** any file operations. Never take the snapshot during or after the installation — it provides no safety value then.
- Atomic transactions mean the installed.db and the filesystem are always in sync. If lpkg crashes mid-install, the next run must detect and roll back the incomplete transaction (check for transactions with no result in the transactions table).
- The graphical privilege escalation (LUNA dialog) is the preferred path. The `sudo` fallback is for non-graphical sessions only. Do not make the sudo path the default — it bypasses the LUNA permission audit system.
- `observe.toml` rules from packages are additive. Never delete or overwrite the user's existing observe.toml rules. The user's customizations always win.

---

*Document: `Volume V / 03_package_manager.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: DL-003, DL-016, DL-017, DL-018, DL-019, DL-027, Volume II/09_filesystem.md*
*Informs: Volume V/06_installer.md, Volume V/07_updater.md, Volume VI/07_release_process.md*
