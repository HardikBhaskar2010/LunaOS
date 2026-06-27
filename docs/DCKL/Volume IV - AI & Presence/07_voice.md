# Mahina — Voice Module
**Volume IV · Chapter 7**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · Voice is disabled by default in v1; this document specifies v1 architecture and v1.5 full implementation

---

## Purpose

This document specifies the **Voice Module** — the optional system that enables LUNA to speak and listen. Voice is not core to Mahina v1 (DL-041), but it is architected correctly from the start so that v1.5 voice activation is an extension, not a rewrite.

This document specifies:
- The v1 voice stubs (disabled, but present in the codebase)
- The v1.5 TTS (Text-to-Speech) architecture
- The v1.5 STT (Speech-to-Text) architecture
- The voice permission model
- LUNA's voice character specification

---

## Overview

```
Voice Module — two directions:

  DIRECTION 1: TTS (LUNA speaks to the user)
  ──────────────────────────────────────────
  Inference Engine generates text
       ↓
  Personality Engine shapes it
       ↓
  Voice Module (TTS) converts to audio
       ↓
  Audio output (PipeWire / ALSA)

  DIRECTION 2: STT (user speaks to LUNA)
  ──────────────────────────────────────
  User presses 🎤 in luna-island
       ↓
  Voice Module (STT) captures microphone
       ↓
  STT converts audio to text
       ↓
  Text sent to Inference Engine as user message

  BOTH directions require user permission (DL-041, Permission Engine)
  BOTH are disabled by default
  NEITHER is required for v1
```

---

## v1 Status: Disabled with Stubs

In Mahina v1, the Voice Module exists in the codebase as **stubs** — the interfaces are defined, the permission gates are in place, but no actual TTS or STT processing occurs.

```c
// v1 stub implementation in luna-ai-d/voice/voice_module.c

bool voice_module_is_enabled() {
    return false;  // Always false in v1
}

voice_status_t voice_module_speak(const char *text) {
    (void)text;
    log_debug("Voice module: disabled in v1. Text not spoken: %.50s", text);
    return VOICE_DISABLED;
}

voice_status_t voice_module_listen(voice_callback_t on_result) {
    (void)on_result;
    log_debug("Voice module: disabled in v1. Microphone not accessed.");
    return VOICE_DISABLED;
}
```

The Personality Engine checks `voice_module_is_enabled()` before calling any voice functions. In v1, this always returns `false`, so the voice path is never entered.

---

## Voice Module Architecture (v1.5)

### TTS Pipeline

```
TTS pipeline (text → audio):

  text_to_speak (string)
         │
         ▼
  Text preprocessor
  (expand abbreviations, handle code blocks,
   normalize punctuation for speech)
         │
         ▼
  TTS engine (Kokoro TTS — recommended, DL-041)
  Input:  text string
  Output: PCM audio buffer (44.1kHz, 16-bit, mono)
         │
         ▼
  Audio output via PipeWire
  (respects system volume curve)
  (never louder than current audio)
         │
         ▼
  User hears LUNA's voice
```

### STT Pipeline

```
STT pipeline (audio → text):

  User presses 🎤 or uses wake word (if configured)
         │
         ▼
  Voice Activity Detector (VAD)
  (lightweight, runs always when listening)
  (detects when the user starts and stops speaking)
         │
         ▼
  Audio capture via PipeWire
  (microphone input, noise-reduced)
  Captured until VAD detects speech end
         │
         ▼
  STT engine (Whisper.cpp local — DL-041)
  Input:  PCM audio buffer
  Output: text transcript
         │
         ▼
  Confidence check
  (if STT confidence < 0.80: show text for user confirmation before sending)
         │
         ▼
  Text sent to Inference Engine as user message
```

### Audio Backend

```
Audio backend: PipeWire
Rationale:
  - PipeWire is the modern Linux audio server (replaces PulseAudio and JACK)
  - Provides both system audio and low-latency audio in one API
  - Supported natively in the Linux kernel we target (6.6.x LTS)
  - Audio routing is user-controllable via PipeWire's graph model

Audio buffer sizes:
  TTS output:   Standard latency (10ms buffer) — OK for speech
  STT input:    Low latency (2–5ms) — needed for responsive VAD

NOTE: PipeWire is an external dependency in v1.5.
      This must be added to the LBUILD dependency list when Voice Module activates.
```

---

## TTS Engine Selection

Three options evaluated (from `luna_personality.md`):

| Engine | Quality | Latency | License | Local | Recommended |
|---|---|---|---|---|---|
| **Kokoro TTS** | High | ~100ms | Apache 2.0 | Yes | ✅ Primary |
| **Piper TTS** | Good | ~50ms | MIT | Yes | 🔵 Fallback |
| **OpenAI TTS** | Excellent | ~300ms + network | Commercial | No | ❌ Not for v1 |

**Decision:** Kokoro TTS for v1.5. Local, high quality, permissive license. Piper TTS as fallback for resource-constrained devices.

```
TODO:
Decision not yet finalized.
Engine selection for TTS requires a hardware validation test.
Kokoro TTS memory usage on 4GB RAM systems must be measured.
Must be a Decision Log entry before v1.5 voice work begins.
```

---

## LUNA's Voice Character

LUNA's voice is not a generic TTS voice. The Voice Module applies **prosody rules** on top of the TTS engine to shape LUNA's distinctive vocal character.

```
LUNA's voice character specification (from luna_personality.md):

  Speed:      1.1× average human speech rate
              Slightly brisk. Confident. Not rushed.

  Tone:       Warm but precise.
              Not bubbly. Not robotic. Somewhere real.

  Register:   Neutral accent.
              No forced "AI voice" affectation.
              No upward inflection at end of statements.

  Emphasis:   Technical terms are spoken at normal speed.
              File names, commands, numbers are spoken clearly.

  Volume:     Respects system volume curve.
              Never louder than the current audio mix.

  Response begin latency:
              < 800ms from end of LLM generation to first audio output.
              (This is why streaming TTS — generating audio while tokens arrive
              — is preferred over waiting for the full response.)
```

### Streaming TTS

For low-latency voice, the TTS engine should begin generating audio before the LLM has finished. This is **streaming TTS**:

```
Streaming TTS flow:

  LLM token: "The "   → buffer (wait for sentence boundary)
  LLM token: "issue " → buffer
  LLM token: "is "    → buffer
  LLM token: "in "    → buffer
  LLM token: "the "   → buffer
  LLM token: "shrink" → buffer
  LLM token: "calculation." → [SENTENCE BOUNDARY DETECTED]
                            → Send "The issue is in the shrink calculation."
                               to TTS engine
                            → Begin audio playback
  LLM token: " Want " → buffer (next sentence)
  ...
```

Sentence boundary detection is done by a lightweight parser looking for `.`, `?`, `!` followed by a capital letter or end of stream.

### Text Preprocessing for Speech

Code blocks and technical content in LUNA's response must be preprocessed before being sent to TTS:

```python
def preprocess_for_speech(text: str) -> str:
    """
    Transform text that contains code/markdown into speakable text.
    """
    # Skip code blocks entirely (don't read out code)
    text = re.sub(r'```[\s\S]*?```', '[code block omitted]', text)
    text = re.sub(r'`([^`]+)`', r'\1', text)  # inline code: read as plain text

    # Expand file extensions to be speakable
    text = text.replace('.c', ' dot C')
    text = text.replace('.md', ' dot markdown')
    # ... etc.

    # Remove markdown formatting
    text = re.sub(r'\*\*(.*?)\*\*', r'\1', text)  # bold
    text = re.sub(r'\*(.*?)\*', r'\1', text)       # italic

    return text
```

---

## Voice Permissions

Voice requires two sensitive permissions. Both must be explicitly granted:

| Permission | What it covers | Grant scope |
|---|---|---|
| `VOICE_OUTPUT` | LUNA may produce audio output | SESSION or PERMANENT |
| `VOICE_INPUT` | LUNA may access the microphone | Per activation (ONCE each press) |

`VOICE_INPUT` (microphone access) is granted for a **single voice input session** — from when the user presses 🎤 to when speech ends. It is never held open persistently.

There is no always-listening mode in v1. There is no wake word activation in v1. Microphone is accessed only when the user explicitly presses the 🎤 button in luna-island.

```
TODO:
Decision not yet finalized.
Wake word support (e.g., "Hey LUNA") is a v2 feature.
Requires continuous microphone access.
This is a significant privacy decision and must be a dedicated Decision Log entry.
```

---

## Voice Mode Behavior

When voice is enabled, the Personality Engine's mode rules apply to voice as well:

| Mode | TTS (LUNA speaks) | STT (user speaks) |
|---|---|---|
| GAMING | ❌ Disabled | ❌ Disabled |
| FOCUS | ❌ Disabled | ❌ Disabled |
| STUDY | ❌ Disabled | ✅ Available |
| DEVSHELL | ✅ Available (for CRISIS only unsolicited) | ✅ Available |
| AMBIENT | ✅ Available | ✅ Available |
| CONVERSING | ✅ Available | ✅ Available |

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Voice disabled by default in v1 | DL-041 | ✅ Accepted |
| Voice is optional and user-opt-in | DL-041 | ✅ Accepted |
| No always-listening / wake word in v1 | DL-041 | ✅ Accepted |
| Microphone: per-press ONCE permission only | Permission Engine | ✅ Accepted |
| TTS engine: Kokoro TTS (primary), Piper (fallback) | This document | 🔵 Draft |
| Audio backend: PipeWire | This document | 🔵 Draft |
| Wake word (v2) | Pending Decision Log entry | 🔵 Draft |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **TTS engine validation.** Kokoro TTS memory usage on 4GB RAM systems must be tested. If it cannot run alongside Ollama without causing OOM, Piper TTS becomes the primary. Must be a Decision Log entry.

2. **Audio backend.** PipeWire is specified here. Must be added to the dependency list and the LBUILD system when v1.5 begins.

3. **Wake word.** "Hey LUNA" is a v2 feature. It requires continuous microphone access and a lightweight on-device keyword detection model (e.g., Porcupine or OpenWakeWord). The privacy implications must be reviewed before implementation.

4. **LUNA voice actor or synthetic.** Is LUNA's voice a fine-tuned TTS voice with a custom character, or a generic TTS voice with prosody rules applied? A custom-trained voice gives LUNA a more distinct identity but requires training data.

5. **Voice transcript.** When LUNA speaks a response, should the text also appear in the luna-island conversation panel? Yes — accessibility requires that spoken content is always also displayed. Must be the default.

---

## AI Context

- Voice Module is **disabled in v1**. All calls to voice functions return `VOICE_DISABLED`. Do not implement TTS or STT functionality in v1 code — only the stubs.
- The stub interface is intentional — it lets the Personality Engine and Conversation Rules be written voice-aware without implementing audio in v1. In v1.5, the stubs are replaced with real implementations.
- Microphone is **never held open**. It is opened when the user presses 🎤 and closed when speech ends. If you see code that holds the microphone open permanently, it is wrong.
- All spoken content must also be displayed as text. Voice cannot be the only output channel — accessibility requires visual equivalents.
- PipeWire replaces PulseAudio. Do not add PulseAudio dependency. If a system doesn't have PipeWire, voice is gracefully disabled.

---

*Document: `Volume IV / 07_voice.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, Volume IV/02_personality_engine.md, Volume IV/05_permission_engine.md, DL-041*
*Informs: Volume IV/08_vision.md, Volume V/08_sdk.md*
