# Mahina — Conversation Rules
**Volume IV · Chapter 6**
**Classification:** Core Architecture — AI & Presence
**Status:** Canonical · Specifies the complete rules for how LUNA conversations begin, proceed, and end

---

## Purpose

This document specifies the **rules governing every LUNA conversation** — from the moment the user initiates contact to the moment the conversation ends and the results are written to memory.

A "conversation" in Mahina is any multi-turn exchange between the user and LUNA where the Inference Engine (LLM) is involved. This is distinct from:
- **Passive observations** — LUNA saying "Build failed. Want the diff?" — which are single messages from the Personality Engine with no LLM involved
- **Expression state changes** — LUNA's Luna Island changing color/animation — which are purely visual, no text

This document governs only full LLM conversations: multi-turn, context-aware exchanges through the Luna Island interface.

---

## Overview

```
Conversation lifecycle:

  [User initiates]
       │
       ▼
  CONVERSATION_OPENED
  (luna-island: FULL_CONVERSATION state)
  (Inference Engine: Ollama started if not running)
       │
       │ User sends first message
       ▼
  TURN_ACTIVE
  (Inference Engine: building prompt)
  (Context Engine: assembling context snapshot)
  (Memory Engine: loading persistent summary)
       │
       │ LLM starts generating
       ▼
  STREAMING
  (Tokens received via org.lunaos.luna.TokenReceived D-Bus signal)
  (luna-island: streaming cursor active)
       │
       │ is_final = true received
       ▼
  TURN_COMPLETE
  (Personality Engine: response shaped)
  (Conversation history updated)
       │
       ├──→ [User sends next message] → TURN_ACTIVE (loop)
       │
       └──→ [User closes or idles out] → CONVERSATION_ENDED
                                          (Memory Engine: session notes updated)
```

---

## Conversation Initiation

### Trigger Paths

A conversation can be initiated in three ways:

**1. Long press on Luna Island** (primary path — DL-034)
- luna-island transitions to `FULL_CONVERSATION` state
- luna-island calls `org.lunaos.luna.Chat()` or begins streaming
- Inference Engine activates

**2. CLI: `luna ask "..."`**
```bash
luna ask "How do I resolve this flex shrink issue?"
# Sends to Inference Engine via D-Bus
# Prints streaming response to stdout
# Non-interactive: no multi-turn
```

**3. Explicit user invocation from any application** (via LUNA SDK — Volume V/08)
```c
// Application code
LunaConversation *conv = luna_conversation_open();
luna_conversation_send(conv, "What's the best way to serialize this struct?");
// Receives streaming response via callback
```

### Pre-Conversation Checklist

Before the first LLM call is made, the Inference Engine runs:

```c
typedef enum {
    CONV_READY,
    CONV_LOADING,   // Ollama starting up
    CONV_ERROR,     // Ollama failed to start
} conversation_readiness_t;

conversation_readiness_t prepare_conversation() {
    // 1. Check if Ollama is running
    if (!ollama_is_alive()) {
        // Start Ollama — lazy initialization (DL-042)
        if (start_ollama() == ERROR)
            return CONV_ERROR;
        // Ollama needs time to load the model
        // luna-island shows GLOW expression (loading)
        wait_for_ollama_ready(timeout_ms = 30000);
        return CONV_LOADING;
    }

    // 2. Load persistent memory (cached from startup if available)
    ensure_persistent_memory_loaded();

    // 3. Get current context snapshot
    refresh_context_snapshot();

    return CONV_READY;
}
```

If Ollama fails to start, luna-island shows a FLASH expression (LUNA_PINK) and displays:
```
"Language model unavailable. Restart and try again?"
```

---

## Prompt Assembly

Every LLM call assembles a prompt from multiple sources. The order matters — it determines what the LLM pays attention to most.

### Prompt Structure

```
Full prompt assembled for each turn:

  [1] SYSTEM PROMPT          ← LUNA's personality and rules (hardcoded)
      (from Personality Engine — Volume IV/02)

  [2] PERSISTENT MEMORY      ← What LUNA remembers about the user
      (from Memory Engine — Volume IV/04)

  [3] CURRENT CONTEXT        ← What the user is doing right now
      (from Context Engine — Volume IV/03)
      [mode, active app, project, session duration]

  [4] CONVERSATION HISTORY   ← Previous turns in this conversation
      (maintained in memory by Inference Engine)

  [5] USER MESSAGE           ← The user's current input
```

### Context Injection Template

```python
CONTEXT_INJECTION = """
## Current Context
Mode: {mode}
Working on: {project_name} ({active_file_extension} file)
Time in session: {session_duration}
Active application: {app_name}
{system_state_section}
"""

SYSTEM_STATE_SECTION = """
## System State (notable)
{notable_items}
"""
# Only included if there are notable system state items:
# - RAM > 80%
# - CPU > 80% sustained
# - Disk > 85%
# - Battery < 20%
```

### Token Budget

```
Token budget for each LLM call:

  System prompt:       ~500 tokens (fixed)
  Persistent memory:  ≤ 8,000 tokens (enforced by Memory Engine)
  Context injection:   ~200 tokens (variable)
  Conversation history: budget = model_context_window - all_above - user_message - response_reserve
  User message:        actual message length
  Response reserve:    1,000 tokens (reserved for LUNA's response)

  For a 4K context window model (e.g., Phi-3 Mini):
    Usable for history:  4096 - 500 - 2000 - 200 - msg_len - 1000
    ≈ 400 tokens of history (very limited)
    → Memory must be kept compact for small models

  For a 32K context window model (e.g., Llama 3.1 8B):
    Usable for history:  32768 - 500 - 5000 - 200 - msg_len - 1000
    ≈ 26,000 tokens of history (many turns of conversation)
```

### Conversation History Management

When the conversation history exceeds the available token budget, older turns are pruned:

```python
def trim_conversation_history(history: list[dict], max_tokens: int) -> list[dict]:
    """
    Trim conversation history to fit within max_tokens.
    Strategy: always keep the FIRST turn (provides original intent)
              and the most recent turns.
              Prune from the middle when needed.
    """
    if count_tokens(history) <= max_tokens:
        return history

    # Keep first turn + N most recent turns
    first_turn = history[0]
    recent_turns = []
    token_count = count_tokens([first_turn])

    for turn in reversed(history[1:]):
        turn_tokens = count_tokens([turn])
        if token_count + turn_tokens <= max_tokens:
            recent_turns.insert(0, turn)
            token_count += turn_tokens
        else:
            break

    return [first_turn] + recent_turns
```

---

## Streaming Protocol

The LLM's response is streamed token-by-token from Ollama to luna-island via D-Bus signals.

### Streaming Flow

```
Streaming sequence:

  Inference Engine                D-Bus                luna-island
       │                            │                       │
       │──LLM call to Ollama ──────►│                       │
       │                            │                       │
       │◄─ token: "The "           │                       │
       │──► TokenReceived("The ", false) ────────────────►│
       │                            │            append "The " to bubble
       │◄─ token: "issue "         │                       │
       │──► TokenReceived("issue ", false) ──────────────►│
       │                            │            append "issue "
       │ (... many tokens ...)      │                       │
       │◄─ token: "." + [DONE]     │                       │
       │──► TokenReceived(".", true) ────────────────────►│
       │                            │            remove cursor, finalize bubble
       │                            │                       │
```

### Streaming Error Handling

If Ollama becomes unresponsive mid-stream:

```c
void handle_stream_timeout() {
    // Emit a final token with is_final = true
    emit_token_received("", true);

    // Show error in conversation
    emit_system_message(
        "Response interrupted. Ollama may have run out of memory."
    );

    // Attempt Ollama restart (non-blocking)
    restart_ollama_async();
}
```

### Token Received Signal Format

```
D-Bus signal: org.lunaos.luna.TokenReceived

Arguments:
  token:    string   — one or more characters (may be a full word or partial)
  is_final: bool     — if true, this is the last token; response is complete
  turn_id:  uint32   — identifies which turn this token belongs to
                       (prevents old tokens from a cancelled request appearing)
```

---

## Multi-Turn State

The Inference Engine maintains conversation state for the duration of the conversation:

```c
typedef struct conversation_state {
    uint64_t     conversation_id;     // Unique ID for this conversation
    uint32_t     turn_count;          // How many turns have occurred
    uint64_t     started_at;          // Unix timestamp
    uint64_t     last_activity_at;    // Last user message or LUNA response

    // Conversation history (circular buffer, capped by token budget)
    luna_turn_t *history;
    uint32_t     history_len;
    uint32_t     history_max;

    // State flags
    bool         is_streaming;        // Currently streaming a response
    bool         user_is_typing;      // Typing indicator active
    bool         ollama_ready;        // Ollama subprocess available

} conversation_state_t;

typedef struct luna_turn {
    bool     is_user;          // true = user, false = LUNA
    char    *content;          // message text
    uint64_t timestamp;        // when this turn occurred
    uint32_t token_count;      // cached for budget calculation
} luna_turn_t;
```

---

## Conversation Timeout and Idle Behaviour

Conversations don't stay open forever. Idle timeout rules:

```
Conversation idle rules:

  User has not sent a message for > 5 minutes:
    → luna-island shows gentle pulse on input field (reminder)
    → No automatic close

  User has not sent a message for > 15 minutes:
    → luna-island shows: "Still there? Closing in 60s."
    → 60-second countdown visible in the Island header

  Countdown expires with no user action:
    → Conversation ends (CONVERSATION_ENDED)
    → luna-island transitions back to AMBIENT state
    → Conversation history cleared from memory

  User closes the conversation explicitly ([×] button):
    → Immediate CONVERSATION_ENDED
    → luna-island: FULL_CONVERSATION → AMBIENT (Collapse animation, 260ms)
```

---

## Conversation End

When a conversation ends (timeout, explicit close, or CLI command terminates), the Inference Engine:

```
CONVERSATION_ENDED sequence:

  1. Mark conversation as ended in state (is_streaming = false)
  2. If streaming was in progress: emit final empty token (is_final = true)
  3. Serialize the conversation history to a compact format
  4. Notify Memory Engine: CONVERSATION_OCCURRED
     (Memory Engine flags this session for summarization at shutdown)
  5. Clear conversation_state_t from memory
  6. If Ollama has been idle for > idle_timeout (configurable, default 10 min):
     Signal Ollama to unload model (reduce RAM usage)
  7. luna-island: transition to AMBIENT
```

---

## Conversation Rules Summary

From `luna_personality.md` and `Volume IV / 02_personality_engine.md`, applied specifically to conversations:

```
During a conversation, LUNA:

  ✅ Responds to what the user actually asked
  ✅ Stays on topic unless the user changes it
  ✅ Admits uncertainty: "I don't have enough context for that"
  ✅ Uses context from the current work session in answers
  ✅ Remembers what was said earlier in this conversation
  ✅ Gives her opinion when asked directly
  ✅ Ends longer explanations with "Does that make sense?" or similar

  ❌ Never fabricates facts
  ❌ Never uses the response to upsell, suggest products, or advertise
  ❌ Never breaks character mid-conversation
  ❌ Never ignores the current context (app, file, project)
  ❌ Never re-introduces herself ("Hi, I'm LUNA, your AI assistant...")
  ❌ Never says "As an AI..." or "As a language model..."
  ❌ Never forgets what was said earlier in the same conversation
```

---

## Current Decisions

| Decision | Source | Status |
|---|---|---|
| Long press initiates full conversation | DL-034 | ✅ Accepted |
| Ollama starts lazily on first conversation | DL-042 | ✅ Accepted |
| Streaming via D-Bus TokenReceived signal | Volume IV/00 | ✅ Accepted |
| Conversation history trimmed from middle (keep first + recent) | This document | ✅ Accepted |
| 15-minute idle → close conversation with countdown | This document | ✅ Accepted |
| Ollama unloads model after 10-minute conversation idle | This document | 🧪 Experimental |
| System prompt is hardcoded | Volume IV/02 | ✅ Accepted |

---

## Open Questions

```
TODO:
Decision not yet finalized.
```

1. **Model context window.** The token budget calculation depends on the active model's context window size. This must be read from Ollama's model info API at startup. How does luna-ai-d query the model's context window? `GET /api/show` in Ollama's REST API — but this needs to be integrated explicitly.

2. **Conversation persistence across reboots.** Currently, conversation history is cleared at CONVERSATION_ENDED. Should LUNA remember the last conversation after a reboot? ("We were talking about the flex shrink bug. Want to continue?") This would require serializing the conversation to disk. Must be a Decision Log entry.

3. **Code block rendering.** If LUNA's response contains a code block (```c ... ```), how does luna-island render it? The FULL_CONVERSATION panel would need a code-aware text renderer with syntax highlighting. Must be specified before the conversation UI is implemented.

4. **Ollama idle timeout.** 10 minutes is the default for Ollama to unload the model after a conversation ends. Should this be configurable per-user in `~/.luna/config/luna.toml`? Likely yes for users with limited RAM.

5. **Typing indicator.** When the user is typing in the conversation panel, should LUNA show a typing indicator ("User is typing...")? This is a UX detail that requires luna-island to watch for keypress events in the input field and signal them. Trivial to implement but must be decided.

---

## AI Context

- Conversations are the **only context where the Inference Engine is active during the session**. All other LUNA behaviors (Presence Engine, Context Engine) are LLM-free. Keep it that way.
- The token budget is the most critical architectural constraint for conversation quality. If the budget is miscalculated, the LLM loses context and starts hallucinating or losing track of the conversation. Always calculate the remaining token budget before each turn.
- The streaming protocol (D-Bus signals) is one-directional: Inference Engine → D-Bus → luna-island. luna-island never writes to the LLM. If you need luna-island to send data to the LLM, it goes through luna-ai-d D-Bus methods, not directly to Ollama.
- The conversation history lives only in the Inference Engine's RAM for the duration of the conversation. It is not written to workflow.db (the Presence Engine does that separately). The Memory Engine gets a notification at conversation end and summarizes at shutdown. These are separate concerns.
- LUNA does not say "As an AI..." or "As a language model..." — ever. These phrases break character and contradict LUNA's identity. If you see this in generated output, the system prompt is not being applied correctly.

---

*Document: `Volume IV / 06_conversation_rules.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Version: 0.1-draft*
*Depends on: Volume IV/00_luna_runtime.md, Volume IV/02_personality_engine.md, Volume IV/03_context_engine.md, Volume IV/04_memory_engine.md*
*Informs: Volume IV/07_voice.md, Volume V/08_sdk.md*
