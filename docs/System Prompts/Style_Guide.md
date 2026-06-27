# Mahina Documentation Style Guide

This document defines the writing standard for every document inside the Divine Collection of Knowledge of Mahina (DCKL).

## Writing Philosophy

Documentation is engineering.

Documentation is architecture.

Documentation is part of the source code.

Every sentence should reduce ambiguity.

Every paragraph should communicate intent.

---

## Write Like

* Linux Kernel Documentation
* LLVM Design Documents
* Rust RFCs
* Chromium Design Docs
* Architecture Decision Records

---

## Never Write Like

* Marketing
* Blog Posts
* Tutorials
* AI-generated filler
* Casual conversation

---

## Tone

Professional.

Direct.

Precise.

Technical.

Confident.

Never dramatic.

Never verbose without reason.

---

## Document Template

Every document should contain:

# Title

## Purpose

## Overview

## Design Philosophy

## Architecture

## Current Decisions

## Technical Details

## Future Improvements

## Open Questions

## AI Context

---

## Missing Information

If information is unavailable:

Write:

TODO:
Decision not yet finalized.

Never invent architecture.

---

## AI Context Rule

Assume Claude Code, Codex, Antigravity, and ChatGPT will use these documents as implementation references.

Write accordingly.

---

## Cross References

Reference related documents whenever possible.

Avoid duplication.

One source of truth.

---

## Diagrams

Prefer ASCII diagrams over paragraphs whenever architecture can be visualized.

---

## Tables

Use tables for:

* configuration
* comparisons
* state machines
* terminology
* APIs
* architecture summaries

---

## Canonical Terminology

Never rename components.

Never invent synonyms.

Use identical names everywhere.

Consistency is mandatory.
