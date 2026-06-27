# Documentation Workflow

## Purpose

This document defines the official documentation workflow used to build and maintain **The Divine Collection of Knowledge of Mahina (DCKL)**.

Its purpose is to ensure documentation remains consistent, scalable, AI-friendly, and architecture-driven throughout the lifetime of the Mahina project.

This workflow is mandatory for every documentation session.

---

# Documentation Philosophy

Documentation is not an afterthought.

Documentation is not generated after implementation.

Documentation is part of the engineering process.

Within Mahina, documentation precedes implementation.

Architecture is documented before code is written.

Every implementation decision must trace back to a documented architectural decision.

The documentation is considered part of the source code.

---

# Documentation Hierarchy

All documentation follows a strict hierarchy.

```
Vision
    ↓
Philosophy
    ↓
Core Laws
    ↓
Identity
    ↓
Architecture
    ↓
Subsystem Design
    ↓
Implementation
    ↓
Code
```

Lower layers may never contradict higher layers.

---

# Documentation Pipeline

The documentation workflow follows this sequence.

```
Project Vision
        ↓
Architecture Decisions
        ↓
Canonical Documents
        ↓
DCKL Documentation
        ↓
Engineering Review
        ↓
Implementation
```

Code never comes before documentation.

---

# AI Workflow

Artificial Intelligence is treated as a documentation department rather than a conversational assistant.

Responsibilities are divided as follows.

## Human Responsibilities

The project owner is responsible for:

* Vision
* Philosophy
* Architecture
* Security decisions
* Graphics decisions
* AI behavior
* UX decisions
* Final approval

## AI Responsibilities

The documentation AI is responsible for:

* Expanding ideas
* Technical writing
* Cross references
* Engineering explanations
* Tables
* Diagrams
* Formatting
* Consistency
* Documentation reviews

AI does not create architecture.

AI documents architecture.

---

# Documentation Department

Claude operates as the Mahina Documentation Department.

Claude is not expected to brainstorm new architecture.

Claude converts architectural decisions into production-quality documentation.

Documentation is generated sequentially according to the Progress Tracker.

---

# Context Management

The project uses two different kinds of context.

## Permanent Context

Always loaded.

* SYSTEM_PROMPT.md
* STYLE_GUIDE.md
* PROGRESS.md

These define how documentation is produced.

---

## Canonical Context

Loaded only when required.

Only documentation relevant to the current volume should be provided.

Example:

When documenting the Scheduler:

Required:

* Vision
* Philosophy
* Core Laws
* Architecture Overview

Not required:

* LUNA Personality
* Glossary
* Graphics documents

Reducing unnecessary context improves generation quality and conserves context window.

---

# Canonical Documents

Canonical documents define Mahina.

They are immutable.

Existing canonical documents should never be rewritten unless explicitly requested.

Future documentation must inherit terminology and philosophy from them.

---

# Non-Negotiable Decisions

Certain architectural decisions are considered permanent.

These should be collected inside a dedicated document.

Example:

```
09_non_negotiables.md
```

Examples include:

* No Wayland
* No Hyprland
* Local First
* Rolling Release
* Motion Creates Presence
* LUNA is a Digital Presence
* Documentation is Code

Whenever documentation conflicts with these decisions, the canonical decision takes precedence.

---

# Decision Log

Every significant architectural decision must be recorded.

Each decision should include:

* Problem
* Alternatives
* Final Decision
* Reasoning
* Consequences

Documentation should reference these decisions instead of repeating them.

---

# Sequential Documentation

Documentation is always written in order.

Claude must determine the next unfinished document using Progress.md.

Claude must never ask:

* Which file next?
* Should I continue?
* Can I generate another document?

Generation continues until the context limit is reached.

---

# Context Exhaustion Policy

When approaching the model context limit:

* Finish the current document.
* Do not begin another document that cannot be completed.
* Output:

Completed Documents

Next Document

Outstanding TODOs

Generation resumes from that exact point in the following session.

---

# Quality Assurance Pipeline

Before a document is considered complete, perform the following review.

```
Generate
      ↓
Self Review
      ↓
Architecture Review
      ↓
Consistency Review
      ↓
Canonical Validation
      ↓
Publish
```

Every document should internally verify:

* Naming consistency
* Terminology
* Cross references
* Architecture
* Scalability
* Missing assumptions

---

# Documentation Standards

Every document should contain whenever applicable:

* Purpose
* Overview
* Design Philosophy
* Architecture
* Current Decisions
* Technical Details
* Future Improvements
* Open Questions
* AI Context

Missing information should never be invented.

Instead:

```
TODO

Decision not yet finalized.
```

---

# AI Generation Rules

The documentation AI must never:

* invent architecture
* invent APIs
* invent security systems
* invent kernel behavior
* invent graphics systems
* invent package manager features

Unknown information must always become TODO items.

---

# Engineering Style

Documentation should resemble professional operating system documentation.

Preferred references include:

* Linux Kernel Documentation
* LLVM Design Documents
* Chromium Design Documents
* Rust RFCs
* FreeBSD Handbook
* Architecture Decision Records

Avoid:

* conversational writing
* marketing
* unnecessary storytelling
* filler
* exaggerated language

---

# Versioning

Documentation itself is versioned.

Example:

DCKL v0.1

↓

DCKL v0.2

↓

DCKL v0.3

Every architectural change increments the DCKL version.

AI coding agents should always be informed which DCKL version they are implementing.

---

# Future Workflow

As the project grows, the documentation system should evolve into a complete engineering knowledge base.

Additional documents should include:

* Terminology Database
* Naming Rules
* Review Checklist
* AI Rules
* Canonical Conflict Detection
* Cross Reference Validation

The long-term objective is for any future developer or AI coding agent to understand Mahina completely by reading DCKL before writing code.

---

# Final Principle

The Divine Collection of Knowledge of Mahina is not simply documentation.

It is the engineering constitution of Mahina.

The documentation defines the architecture.

The architecture defines the implementation.

The implementation defines the code.

Therefore:

Documentation is code.

Architecture is law.

Consistency is mandatory.
