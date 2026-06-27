# Mahina — Networking Architecture
**Volume II · Chapter 10**
**Classification:** Core Architecture — Network Stack
**Status:** Active · Reference for network service implementation

---

## Purpose

This document specifies the Mahina networking architecture: the network stack, interface management, DNS configuration, firewall policy, network-related services, and the rules governing what network traffic Mahina initiates automatically versus what requires explicit user action.

---

## Overview

Mahina uses a standard Linux network stack managed by NetworkManager in userspace. The design priority for networking is: **user-initiated traffic only**. Mahina never initiates outbound connections without the user's knowledge or explicit instruction. This is a direct consequence of Core Law II (Local First) and Core Law V (User Owns the Machine).

---

## Design Philosophy

**No automatic outbound traffic.** After a clean boot, Mahina initiates no outbound network connections automatically. NetworkManager may probe for connectivity (DHCP, DNS), but no Mahina service contacts remote servers, calls home, checks for updates, or sends telemetry. All outbound traffic from Mahina infrastructure requires explicit user instruction.

**Cloud is opt-in (Law II).** The network exists because the user may want it — for package updates, cloud bridge AI queries, file downloads. It does not exist to serve Mahina's infrastructure needs.

**Internal IPC is localhost-only.** All Mahina inter-process communication that uses TCP/IP is bound to `127.0.0.1`. No Mahina service opens a port on a network interface. See `07_ipc.md` and `08_security.md`.

---

## Architecture

### Network Stack

```
User Applications
        │ (socket API)
        ▼
Linux Network Stack (kernel)
        │
        ├── IPv4 (CONFIG_INET=y)
        ├── IPv6 (CONFIG_IPV6=y)
        └── netfilter / nftables (CONFIG_NETFILTER=y, CONFIG_NF_TABLES=y)
                │
                ▼
NetworkManager (userspace)
        │
        ├── Wired (ethernet — dhclient or built-in DHCPv4)
        ├── Wireless (Wi-Fi — wpa_supplicant or iwd)
        └── DNS (systemd-resolved equivalent — see DNS section)
```

### Network Services

### Network Services

| Service | Role | Started by |
|---|---|---|
| nftables | Firewall — starts before NetworkManager | luna-init Stage 4 (first) |
| NetworkManager | Interface management, DHCP, Wi-Fi | luna-init Stage 4 |
| wpa_supplicant or iwd | Wi-Fi authentication (DL-013 — criteria defined, choice pending) | NetworkManager subprocess |
| ntpd or chrony | Time synchronization (DL-015) | luna-init Stage 4 |

```
DL-013 NOTE:
Wireless backend criteria (DL-013): maximum hardware compatibility and strong performance.
wpa_supplicant vs. iwd evaluation is ongoing. Both are compatible with NetworkManager.
A follow-up Decision Log entry is required after hardware compatibility testing.
```

### DNS Configuration

**Resolved (DL-014):** Version 1.x uses the existing Linux DNS resolver.

NetworkManager writes `/etc/resolv.conf` with DHCP-provided upstream DNS servers. No local caching resolver is used in v1.

A future **LunaDNS** service may replace this after sufficient architectural research. LunaDNS would add DNS-over-TLS, local caching, and privacy features. It is a post-v1 research item.

```toml
# NetworkManager.conf — DNS passthrough
[main]
dns = default   # Write /etc/resolv.conf with DHCP-provided servers
```

### Firewall Architecture

The firewall is implemented via nftables (see `08_security.md` for the full ruleset). From the network architecture perspective:

**Default inbound policy: DROP** (except established connections and loopback)
**Default outbound policy: ACCEPT** (per-application restriction via AppArmor, not firewall rules)

The firewall is a luna-init service at Stage 4, loaded before NetworkManager. This ensures the firewall is active before any network interface comes up.

```
Luna-init Stage 4 service order:
  1. nftables (firewall rules loaded)
  2. NetworkManager (interfaces come up — firewall already active)
```

This order is mandatory. Reversing it would create a window where the network is active without firewall protection.

---

## Technical Details

### NetworkManager Configuration

NetworkManager configuration lives in `/etc/NetworkManager/`:

```
/etc/NetworkManager/
├── NetworkManager.conf          # Main configuration
└── system-connections/          # Saved connection profiles (TOML-like keyfile format)
    └── home-wifi.nmconnection   # Example saved Wi-Fi profile
```

```
TODO:
Decision not yet finalized.
Reason: NetworkManager uses its own keyfile format, not TOML.
DL-008 specifies TOML for all Mahina configuration files.
NetworkManager's keyfile format is not TOML-compatible.
Options:
  A: Accept NetworkManager's keyfile format as an upstream exception (Law I permits
     using upstream tools we understand).
  B: Write a configuration translator: user edits TOML, a service converts to
     NetworkManager keyfile format.
Option A is simpler and more maintainable. NetworkManager is a well-understood tool.
The Law I exception for upstream tools that are fully understood applies here.
This must be a documented exception in the Decision Log.
```

**NetworkManager.conf:**

```ini
[main]
plugins = keyfile
dns = none               # Mahina manages DNS separately (via Unbound — TODO)

[connection]
wifi.powersave = 2       # Disable Wi-Fi power saving for lower latency

[logging]
backend = file
level = WARN
```

### Interface Naming

Mahina uses kernel-assigned interface names (e.g., `eth0`, `wlan0`) rather than predictable network interface names (`enp3s0`, `wlp2s0`). Predictable naming requires `udev` rules and adds complexity without a clear benefit for the Mahina use case.

```
TODO:
Decision not yet finalized.
Reason: Interface naming convention (kernel names vs. predictable names) has
not been formally decided.
Predictable names are the modern default and NetworkManager works well with them.
Kernel names are simpler and require no udev rules.
Since Mahina does not use udev (udevd would be a system service dependency —
check whether devtmpfs is sufficient), predictable names may not be achievable
without additional tooling.
This must be resolved as part of the device management architecture.
```

### Network-Related sysctl Settings

```toml
# /etc/luna/sysctl.toml
[net.core]
rmem_max          = 134217728    # Maximum receive socket buffer (128 MB)
wmem_max          = 134217728    # Maximum send socket buffer (128 MB)
netdev_max_backlog = 5000        # Increase network device input queue

[net.ipv4]
tcp_fastopen       = 3           # Enable TCP Fast Open for clients and servers
tcp_congestion_control = "bbr"   # BBR congestion control — better throughput
tcp_notsent_lowat  = 16384       # Reduce TCP send buffer latency

[net.ipv6]
disable_ipv6 = 0                 # IPv6 enabled (not disabled)
```

BBR congestion control (`net.ipv4.tcp_congestion_control = bbr`) provides better throughput and lower latency than the default CUBIC, especially useful when `lpkg` is downloading packages or Ollama is pulling model weights.

### Outbound Traffic Policy

This table defines exactly what outbound network traffic Mahina initiates and under what conditions:

| Traffic | Initiator | Trigger | User Control |
|---|---|---|---|
| DHCP | NetworkManager | Network interface comes up | Automatic — required for network access |
| DNS queries | NetworkManager / resolv.conf | Any DNS lookup | Automatic — required for network access |
| NTP (clock sync) | ntpd / chrony (DL-015) | Boot, then periodic | Automatic — standard NTP behavior |
| Package index | lpkg | `lpkg update` (manual) | ✅ Manual only |
| Package download | lpkg | `lpkg install <pkg>` (manual) | ✅ Manual only |
| Model download | Ollama | `ollama pull <model>` (manual) | ✅ Manual only |
| Cloud bridge AI | luna-ai-d | `luna bridge --send` (manual) | ✅ Manual, explicit opt-in |
| Crash reports | — | Never | ❌ Does not exist |
| Telemetry | — | Never | ❌ Does not exist |
| Auto-updates | — | Never | ❌ Mahina never auto-updates |

```
TODO:
Decision not yet finalized.
Reason: NTP is automatic (DL-015: use existing Linux NTP service). This is
an outbound connection that runs without explicit user instruction.
This is accepted as necessary infrastructure (same category as DHCP and DNS).
The user may disable NTP via config if desired, but it is on by default.
```

### LAN and Local Network

Mahina makes no assumptions about the local network topology. It does not:
- Run mDNS/Avahi by default (zero-configuration networking — opt-in)
- Run SSH server by default (must be explicitly installed and enabled)
- Run any other listening service on network interfaces

After a clean install, `nmap` scanning the Mahina host from another machine on the LAN should show no open ports.

---

## Future Improvements

| Improvement | Target | Notes |
|---|---|---|
| iwd adoption decision | v1 | Replace wpa_supplicant if iwd is chosen |
| Unbound DNS with DNS-over-TLS | v1 | Privacy-aligned DNS resolver |
| NTP policy Decision Log entry | v1 | Decide NTP behavior |
| NetworkManager TOML wrapper | v2 | If TOML consistency is required |
| `luna network` CLI | v1 | Status, connect, disconnect commands wrapping nmcli |
| Wi-Fi captive portal handling | v1.5 | Detect and handle captive portals |
| VPN support | v1.5 | OpenVPN or WireGuard via NetworkManager plugin |
| mDNS opt-in | v1.5 | Avahi disabled by default, enabled via luna network --mdns on |

---

## Open Questions

1. **Wi-Fi backend.** **Partially resolved (DL-013).** Criteria defined: maximum hardware compatibility and strong performance. wpa_supplicant vs. iwd evaluation ongoing. Follow-up DL entry required.

2. **DNS resolver.** **Resolved (DL-014).** NetworkManager passthrough: write `/etc/resolv.conf` with DHCP-provided servers. LunaDNS is a post-v1 research item.

3. **NTP synchronization policy.** **Resolved (DL-015).** Use existing Linux NTP service (ntpd or chrony). Starts at Stage 4.

4. **Interface naming.** Kernel names vs. predictable names. Depends on udev/devtmpfs device management decision.

5. **udev.** Whether Mahina runs udevd or relies on devtmpfs alone has not been decided.

6. **NetworkManager TOML exception.** If NetworkManager’s keyfile format is accepted as an upstream exception to DL-008, this must be documented in the Decision Log.

---

## AI Context

An AI agent implementing Mahina networking must understand:

- After a clean boot with no user interaction, Mahina initiates no outbound connections except DHCP and DNS (required for network functionality). Everything else is user-triggered.
- No Mahina service opens a port on a network interface. `localhost:7734` and `localhost:11434` are loopback only. External port scans of a Mahina host show no open ports.
- The firewall is loaded before NetworkManager in the service startup order. This is mandatory — do not change this order.
- `luna-ai-d` never makes outbound calls except (a) localhost, and (b) cloud bridge via explicit user command. If implementing luna-ai-d, do not add any automatic outbound calls.
- The NTP strategy is undecided. Do not implement a background NTP sync without a Decision Log entry. Until decided, the clock is set from hwclock at boot.
- DNS resolution depends on whether Unbound is adopted. Until that decision is made, NetworkManager writes to `/etc/resolv.conf` directly. Do not hardcode DNS resolver paths.
- NTP runs automatically (DL-015). This is an accepted infrastructure outbound connection alongside DHCP and DNS. It is not a violation of the “no automatic outbound” policy.
- DNS uses NetworkManager passthrough to `/etc/resolv.conf` (DL-014). No local Unbound daemon runs in v1.
- Auto-updates do not exist in Mahina. Any code that schedules automatic downloads or package updates is a violation of Core Law V.

---

*Document: `Volume II / 10_networking.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.2-draft*
*Depends on: architecture_overview.md, linux_architecture.md, init_system.md, security.md, ipc.md, core_laws.md (Law II, V), decision_log.md (DL-013, DL-014, DL-015), non_negotiables.md*
*Supersedes: v0.1-draft (DNS and NTP open questions now resolved)*
