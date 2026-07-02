# MinUI C Style Guidelines

This document sets coding standards for MinUI. All modules must follow these rules uniformly.

## 1. Single Responsibility
Each module has one clearly stated purpose, reflected in its filename. Use modular `.c` and `.h` pairs; keep public interfaces in headers.

## 2. Module Header
Every file starts with a comment block explaining its purpose and design context.

## 3. Internal Ordering
Within a source file:
1. Public headers, then private headers
2. Preprocessor macros / Constants
3. Public functions
4. Private static helper functions

## 4. File Size Limit
**500 SLOC maximum.** Blank lines, comments, and docstrings do not count. Crossing this requires splitting the file.

## 5. Function Size & Complexity
- **20 SLOC maximum** (excluding blank lines, comments, docstrings).
- **3 indentation levels maximum.** Deeper nesting requires extracting a helper function.
- All function signatures must be fully typed.
- Don't put business logic in `main()`; avoid `goto` except for unified cleanup/resource release.

## 6. Dependencies & Memory Safety
Prefer the standard library. Validate memory usage with AddressSanitizer/Valgrind. Prefer stack memory allocation; use heap allocation (`malloc`/`calloc`) only when necessary.

## 7. Function Comments
Every function declaration in headers must document its behavior, parameters, and return value (Doxygen style `///` or `/** */`):
```c
/// One-line summary.
/// @param arg Description.
/// @return Description.
```

## 8. Network Safety
All outbound network calls must specify explicit timeouts.
