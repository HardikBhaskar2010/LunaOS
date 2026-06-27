# LunaOS — AI Models
**Volume IV · Chapter 10**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · This document defines which AI models LunaOS uses, how they are managed, and how LUNA handles resource constraints

---

## Purpose

This document specifies the **AI model layer** of LunaOS: which models are used for language understanding, which for vision, which for voice, how they are installed and updated, how resource limits are enforced, and how LUNA behaves when models are unavailable.

This is the document that answers: "What is LUNA actually running under the hood, and why?"

---

## Overview

```
AI model landscape in LunaOS:

  ┌────────────────────────────────────────────────────────┐
  │  LunaOS AI Model Stack                                  │
  │                                                          │
  │  ┌──────────────────────────────────────────────────┐   │
  │  │  Language Models (LLM)                           │   │
  │  │  Primary: Phi-3 Mini 3.8B (default, 4-bit quant) │   │
  │  │  Secondary: Llama 3.1 8B (user upgrade)          │   │
  │  │  Runtime: Ollama                                  │   │
  │  └──────────────────────────────────────────────────┘   │
  │                                                          │
  │  ┌──────────────────────────────────────────────────┐   │
  │  │  Vision Models (v2+, optional)                   │   │
  │  │  Candidate: Phi-3 Vision / LLaVA                 │   │
  │  │  Runtime: Ollama (multimodal)                    │   │
  │  │  Status: Disabled in v1                          │   │
  │  └──────────────────────────────────────────────────┘   │
  │                                                          │
  │  ┌──────────────────────────────────────────────────┐   │
  │  │  Speech Models (v1.5+, optional)                 │   │
  │  │  TTS: Kokoro TTS / Piper TTS                     │   │
  │  │  STT: Whisper.cpp (tiny or base model)           │   │
  │  │  Runtime: Standalone (not Ollama)                │   │
  │  │  Status: Disabled in v1                          │   │
  │  └──────────────────────────────────────────────────┘   │
  │                                                          │
  │  ┌──────────────────────────────────────────────────┐   │
  │  │  Lightweight Models (always on)                  │   │
  │  │  VAD: Silero VAD (voice activity detection)      │   │
  │  │  Classifier: Rule-based + tiny embedding model   │   │
  │  │  Runtime: Native library (not Ollama)            │   │
  │  │  Status: v1.5 (voice) / v1 (rule-based only)     │   │
  │  └──────────────────────────────────────────────────┘   │
  └────────────────────────────────────────────────────────┘
```

---

## Language Models

### Default Model: Phi-3 Mini 3.8B

```
Model: Phi-3 Mini 3.8B (4-bit quantized)
Source: Microsoft (MIT License)
Format: GGUF (via Ollama)
Quantization: Q4_K_M (4-bit, K-means quantized, medium quality variant)

Resource requirements:
  RAM:    ~2.5 GB (4-bit quantized, loaded)
  Disk:   ~2.1 GB (model file)
  CPU:    Inference via llama.cpp (CPU or GPU)
  VRAM:   Optional — GPU offload if VRAM available (DL-026)

Why Phi-3 Mini:
  - Small enough to run on 4GB RAM systems with the rest of the OS
  - Microsoft MIT license — fully permissive for distribution
  - Strong instruction following for its size
  - Runs via Ollama — no additional integration work
  - Fast response time on CPU (2–4 tokens/sec on modern laptop)
```

**DL-006 note:** Phi-3 Mini is listed as `Deprecated` in the decision log (DL-006) because model selection is ongoing. This document updates the recommendation. A new Decision Log entry (post-AR-004) should formalize the v1 default model.

```
TODO:
Decision not yet finalized.
DL-006 marks Phi-3 Mini as the current default but deprecated pending
final model selection. A new DL entry replacing DL-006 must be created
before v1 ships. Phi-3 Mini 3.8B Q4_K_M is the current recommendation.
```

### Secondary Model: Llama 3.1 8B

```
Model: Llama 3.1 8B (4-bit quantized)
Source: Meta (Llama 3.1 Community License)
Format: GGUF (via Ollama)

Resource requirements:
  RAM:    ~5.5 GB (4-bit quantized)
  Disk:   ~4.7 GB
  VRAM:   8GB+ for full GPU acceleration

Availability:
  Not installed by default.
  User may install via: luna model install llama3.1
  Recommended for systems with 16GB+ RAM.
  LUNA automatically switches to Llama 3.1 if available
  and user has set it as preferred in luna.toml.
```

### Model Selection Hierarchy

```
LUNA selects the language model at startup using this hierarchy:

  1. User-configured model in ~/.luna/config/luna.toml:
       [ai]
       preferred_model = "llama3.1"

  2. If preferred_model not installed: fall back to best available
     (scan Ollama model list via GET /api/tags)

  3. If no models installed: pull phi3:mini automatically on first boot
     (requires internet connection on first setup — documented in installer)

  4. If Ollama fails to connect: luna-ai-d enters DEGRADED mode
     (Context Engine + Presence Engine still run — no LLM functionality)
```

### Model Performance Targets

```
Target response performance for default model (Phi-3 Mini, CPU):

  First token latency:  < 3 seconds
  Sustained throughput: ≥ 3 tokens/second
  Full response time (typical 50-word response): < 20 seconds

These targets must be validated on the minimum recommended hardware:
  RAM: 8GB
  CPU: Intel Core i5 8th gen or equivalent
  GPU: None (CPU-only inference)
```

---

## Model Security

To prevent supply-chain attacks via compromised LLM models, LunaOS enforces strict hash pinning. Ollama is not trusted to validate its own downloads independently.

```
Model Hash Verification Flow:

  1. User runs `luna model install llama3.1` (or system installs default on first boot).
  2. `luna-shell` delegates the request to `lpkg`.
  3. `lpkg` reads the Ed25519-signed system manifest (`/etc/luna/models.toml`).
  4. `lpkg` verifies the signature of the manifest using the OS public key.
  5. `lpkg` extracts the expected SHA-256 hash for the requested model.
  6. `lpkg` instructs Ollama to pull the model.
  7. Before Ollama can load the model into RAM, `lpkg` performs a hash check against the downloaded blob.
  8. If the hash fails, `lpkg` deletes the blob and logs a FATAL security error.

  This ensures that only models explicitly verified and signed by the LunaOS core team can be executed by the AI Runtime.
```

---

## Model Management

### Model Storage

```
Model storage location:

  Primary:   ~/.ollama/models/  (Ollama default — managed by Ollama)
  Size:      Variable — phi3:mini = 2.1GB, llama3.1 = 4.7GB
  Quota:     No hard quota in v1. User may fill disk with models.
             TODO: Add disk space check before model download in lpkg.

  Metadata:  ~/.luna/config/models.toml (LUNA's model registry)
```

```toml
# ~/.luna/config/models.toml — LUNA's view of installed models

[models]
default_llm      = "phi3:mini"
preferred_llm    = ""  # user override — empty means use default
vision_model     = ""  # empty = vision disabled
tts_model        = ""  # empty = TTS disabled
stt_model        = ""  # empty = STT disabled

[[models.installed]]
name     = "phi3:mini"
type     = "llm"
source   = "ollama"
version  = "3.8B"
quant    = "Q4_K_M"
size_gb  = 2.1
verified = true  # hash verified on download

[[models.installed]]
name     = "llama3.1"
type     = "llm"
source   = "ollama"
version  = "8B"
quant    = "Q4_K_M"
size_gb  = 4.7
verified = true
```

### Model CLI

```bash
# luna model — model management CLI

luna model list
  → "Installed models:"
    "  phi3:mini     (3.8B, 4-bit)  — 2.1 GB  [default]"
    "  llama3.1      (8B, 4-bit)    — 4.7 GB"

luna model install llama3.1
  → Calls: ollama pull llama3.1
  → Verifies hash
  → Updates models.toml

luna model remove llama3.1
  → Prompts: "Remove llama3.1? This will delete 4.7 GB. Continue? [y/N]"
  → On confirm: ollama rm llama3.1, updates models.toml

luna model set-default llama3.1
  → Updates preferred_llm in models.toml

luna model status
  → "Active model: phi3:mini (loaded)"
    "Ollama: running, port 11434"
    "RAM used by model: 2.4 GB"
```

---

## Resource Management

### Memory Budget (DL-042)

LUNA's AI stack does not reserve a fixed memory allocation. Instead, it uses a **dynamic priority model**:

```
Memory allocation rules:

  1. OS gets first claim: kernel + system services (~1GB minimum)
  2. Active applications get second claim (user is using them)
  3. Ollama (model in RAM) uses what remains
  4. If system RAM pressure occurs:
     → OS starts swapping or invoking OOM killer
     → Ollama is in the OOM killer's crosshairs (large process)
     → luna-ai-d detects Ollama death via health check
     → Enters DEGRADED mode until Ollama can restart

  This is by design (DL-042): we do not fight the OS for memory.
  LUNA's intelligence is sacrificed before the user's applications.
```

### OOM Recovery Sequence

```
Ollama killed by OOM killer:

  1. luna-ai-d health check fails: GET /api/tags → connection refused
  2. luna-ai-d emits ExpressionChanged: FLASH, LUNA_PINK (brief warning)
  3. luna-ai-d enters DEGRADED mode:
       - Presence Engine: continues (not Ollama-dependent)
       - Context Engine: continues
       - Inference Engine: suspended
       - Personality Engine: template-only mode (no LLM)
  4. luna-ai-d emits ExpressionChanged: PULSE_GENTLE, LUNA_AMBER
     (degraded but operational)
  5. luna-ai-d monitors RAM availability (via Context Engine)
  6. When RAM available ≥ model_size_gb + 1.5GB buffer:
       → Attempt Ollama restart
       → Log: "Ollama restarted after OOM recovery"
       → luna-ai-d emits ExpressionChanged: PULSE_GENTLE, LUNA_GREEN
       → LUNA returns to normal mode
```

### Swap Interaction (DL-038)

```
Swap configuration effects on AI models:

  zram compressed swap (active when RAM < swap threshold):
    → Ollama may be partially swapped to zram
    → Performance degrades but model stays "available"
    → Token generation slows from 3 tok/s to ~1 tok/s
    → LUNA still functional, but slower

  Swapfile (active when zram is full):
    → Ollama partially swapped to disk
    → Performance severely degrades (< 0.5 tok/s)
    → luna-ai-d detects slow response (> 60s per token)
    → Enters DEGRADED mode proactively to free RAM

  luna-ai-d response latency monitoring:
    if time_to_first_token > 30 seconds:
        log_warning("LLM response too slow — possible memory pressure")
        emit_expression(PULSE_GENTLE, LUNA_AMBER)  # subtle warning
    if time_to_first_token > 60 seconds:
        abort_inference()
        enter_degraded_mode()
```

---

## DEGRADED Mode

When the LLM is unavailable, LUNA operates in **DEGRADED mode**:

```
DEGRADED mode behavior:

  What still works:
    ✅ Presence Engine (mode detection, expressions)
    ✅ Context Engine (context observation)
    ✅ Personality Engine (template responses only)
    ✅ Automation Engine (suggestion library, no LLM-generated suggestions)
    ✅ Permission Engine
    ✅ Memory Engine (workflow.db writing continues)

  What doesn't work:
    ❌ LLM conversations
    ❌ Complex reasoning
    ❌ Free-form LUNA responses
    ❌ Summarization at session end

  LUNA's behavior in DEGRADED mode:
    Luna Island: PULSE_GENTLE, LUNA_AMBER (degraded, not broken)
    If user tries to start conversation:
      LUNA: "Language model unavailable. System context awareness active."
    Template responses (build failed, memory high, etc.) still work.
    Pattern observations (git push reminder, etc.) still work.
```

---

## Model Update Policy

```
Model update rules:

  LunaOS does NOT auto-update AI models.
  Rationale: Model updates can change behavior significantly.
             The user should control when a model changes.

  Notification behavior:
    When lpkg checks for updates, it queries Ollama for model updates.
    If a model update is available:
      → LUNA displays: "phi3:mini has an update available. Install?"
      → User confirms → luna model update phi3:mini
      → Old model kept for 7 days, then pruned (configurable)

  Manual update:
    luna model update phi3:mini
    luna model update --all

  Rollback:
    If the user is unhappy with an updated model:
    luna model rollback phi3:mini
    (Restores the previous version if still available on disk)
```

---

## Model Security

```
Model security rules:

  1. Models are downloaded via Ollama's HTTPS endpoint.
     Hash verification is performed after download.
     Corrupted or tampered models refuse to load.

  2. Models from third-party registries (not ollama.com) require
     explicit user confirmation before download.
     (Uses the same permission dialog as lpkg third-party packages)

  3. Models run in Ollama's process space.
     Ollama runs under the user's UID (not root, not system service).
     AppArmor profile limits what Ollama can access.

  4. LUNA never loads a model directly (bypassing Ollama).
     All LLM calls go through Ollama's REST API.
     This provides a process boundary between luna-ai-d and the model.
```

---

## Minimum Hardware Requirements

Based on the default model (Phi-3 Mini, 4-bit quantized):

```
Minimum hardware for AI features to function:

  RAM:    8 GB total system RAM
          (4 GB for OS + applications, 2.5 GB for model, 1.5 GB buffer)

  CPU:    x86-64, with at least 4 cores
          AVX2 support strongly recommended (llama.cpp performance)

  Disk:   5 GB free for AI model storage
          (2.1 GB model + 2 GB working space)

  GPU:    Not required. Optional for acceleration (DL-026).
          NVIDIA/AMD GPU with ≥ 4 GB VRAM provides significant speedup.

Recommended hardware for good AI experience:

  RAM:    16 GB
  CPU:    Modern 6+ core (Ryzen 5000+, Intel 12th gen+)
  GPU:    6 GB VRAM (full GPU acceleration for phi3:mini)
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Ollama as the LLM runtime | DL-006 | ✅ Accepted |
| Dynamic memory — no fixed reservation | DL-042 | ✅ Accepted |
| Default model: Phi-3 Mini 3.8B | DL-006 (deprecated — needs new entry) | 🟡 Deprecated |
| Model selection: pending final DL entry | This document | 🔵 Draft |
| No auto-update of AI models | This document | ✅ Accepted |
| GPU offload optional (not required) | DL-026 | ✅ Accepted |
| Vision model selection (v2) | This document | 🔵 Draft |
| TTS engine: Kokoro TTS | Volume IV/07 | 🔵 Draft |
| STT engine: Whisper.cpp | Volume IV/07 | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Default model final selection.** DL-006 is deprecated. A new Decision Log entry must finalize the v1 default LLM. Recommendation: Phi-3 Mini 3.8B Q4_K_M via Ollama. Must be a Decision Log entry.

2. **Model download on first boot.** If no model is installed, LUNA downloads phi3:mini automatically. This requires internet access during first boot. The installer must inform the user of this requirement. What happens if first boot is offline? Must be documented in the installer spec (Volume V/06).

3. **AVX2 requirement.** llama.cpp performs best with AVX2. Older CPUs without AVX2 will run extremely slowly. Should LunaOS require AVX2 as a minimum hardware feature? Must be a hardware requirements Decision Log entry.

4. **Model disk quota.** Users can install multiple large models. The Context Engine should monitor disk usage and warn when model storage exceeds a threshold. Currently specified as a TODO — must be implemented in lpkg or luna-settings.

5. **Model hash verification source.** Who maintains the trusted hash list for model verification? If Ollama's hash is compromised, LunaOS would download a malicious model. Consider: pin model hashes in LunaOS's own package manifest. Must be a security Decision Log entry.

---

## AI Context

- The LLM is **not always running**. It starts lazily when the first conversation is initiated (DL-042). The Presence Engine, Context Engine, and template-based responses work without the LLM.
- DEGRADED mode is a real state that must be tested. Do not write code that assumes Ollama is always available. Every path through the Inference Engine must handle the "Ollama is down" case.
- The default model (Phi-3 Mini) is small for a reason: LunaOS targets hardware real people have (8GB RAM laptops). A model that requires 16GB to run is not a default model.
- Ollama's REST API is the only interface to the LLM. Never spawn llama.cpp or any model binary directly from luna-ai-d. The process isolation Ollama provides is a security feature.
- Model updates change behavior. If a new model version makes LUNA sound different, that's a behavioral change that the user should control. Hence no auto-update.

---

*Document: `Volume IV / 10_ai_models.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, DL-006, DL-026, DL-042*
*Informs: Volume VII/implementation_roadmap.md*
