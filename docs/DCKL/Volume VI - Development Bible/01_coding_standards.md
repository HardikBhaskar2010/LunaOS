# Mahina — Coding Standards
**Volume VI · Chapter 01**
**Classification:** Development Bible — Engineering Standards
**Status:** Canonical · Code that violates these standards will not be merged

---

## Purpose

This document defines the C and Rust coding standards for Mahina. It ensures that the entire repository reads as if it were written by a single engineer.

Mahina targets resource-constrained hardware (v1). Memory efficiency, deterministic execution, and extreme clarity are favored over cleverness.

---

## Language Scope

- **Bootloader:** Assembly + C
- **Kernel & Core Init (Stage 0-1):** C17
- **Compositor & Protocol (Stage 2-3):** C17
- **Desktop & Userland (Above Compositor):** Rust
- **Tooling:** Shell, Python (build helper scripts)
- **C++ is forbidden** in the Mahina base system.

---

## C Coding Standards

### 1. Naming Conventions

- **Structs and Typedefs:** `snake_case_t` (e.g., `lgp_surface_t`)
- **Functions:** `namespace_verb_noun` (e.g., `lgp_surface_create`)
- **Variables:** `snake_case` (e.g., `surface_count`)
- **Macros and Constants:** `UPPER_SNAKE_CASE` (e.g., `LGP_MAX_SURFACES`)
- **Enums:** `NAMESPACE_VALUE` (e.g., `LGP_LAYER_SHELL`)

### 2. Memory Management

Mahina strictly prohibits haphazard dynamic allocation in hot paths.

- **Hot Paths:** The compositor render loop and LGP message parser must not call `malloc()` or `free()`. Memory must be pre-allocated or pooled.
- **Ownership:** Every `malloc()` must have a clearly documented owner responsible for `free()`. If a function returns allocated memory, its name must imply allocation (e.g., `alloc`, `create`, `duplicate`).
- **Initialization:** All structs must be zero-initialized upon creation (`calloc` or `= {0}`).

### 3. Error Handling

- Functions that can fail must return a status code (e.g., `int` where `0` is success and `< 0` is an error code) or a nullable pointer.
- Out parameters must be used for returning data when the return value is used for status.
- **No silent failures:** Every error must be logged or propagated.
- Avoid deep `if` nesting using early returns (`goto cleanup` is permitted for resource freeing on error paths).

```c
// Correct error handling pattern
int lgp_surface_create(lgp_client_t *client, lgp_surface_t **out_surface) {
    if (!client || !out_surface) return -EINVAL;

    lgp_surface_t *surf = calloc(1, sizeof(lgp_surface_t));
    if (!surf) return -ENOMEM;

    if (lgp_client_register_surface(client, surf) != 0) {
        free(surf);
        return -EFAULT;
    }

    *out_surface = surf;
    return 0;
}
```

### 4. File Structure

Every `.c` file must have a corresponding `.h` file.
Headers must contain include guards:
```c
#ifndef MAHINA_LGP_SURFACE_H
#define MAHINA_LGP_SURFACE_H
// ...
#endif
```
Only expose symbols in headers that are strictly part of the public API. Prefix internal symbols in `.c` files with `static`.

### 5. Memory Safety & Tooling

To mitigate the inherent risks of C17 in highly privileged daemons (`luna-init`, `lpkg`, `lgp-compositor`), the following security tooling is strictly mandatory:

- **Sanitizers:** All Debug builds must be compiled with AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`).
- **Static Analysis:** `clang-tidy` is mandatory in the CI pipeline for all pull requests. Code with static analysis warnings will not be merged.
- **Parsers:** All parsers for external input (TOML configurations, LGP TLV packets, `lpkg` manifests) must use strictly bounded buffers. Fuzzing via `AFL++` is required for all parser modules.

---

## Rust Coding Standards (v2)

- Follow standard `rustfmt` and `clippy` guidelines.
- `#![deny(unsafe_code)]` must be applied to all modules unless explicitly interfacing with FFI (C code).
- FFI boundaries must be tightly scoped in `sys/` modules.
- `unwrap()` and `expect()` are prohibited in production daemon code (e.g., `luna-ai-d`). All `Result` types must be handled.

---

*Document: `Volume VI / 01_coding_standards.md`*
*Author: Hardik Bhaskar (Luna Kitsune)*
*Status: Active*
