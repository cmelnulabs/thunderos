Code Quality & Refactoring
=====================================

Overview
--------

ThunderOS is a modular operating system project. High code quality is essential for reliability, maintainability, and extensibility. This document outlines mandatory coding standards and refactoring principles for all contributions.

Coding Standards
----------------

All code must follow these rules:

**Mandatory Rules:**

1. **Descriptive Variable Names**
   - Use full, descriptive names that indicate purpose (e.g., `process_id`, `interrupt_vector`).
   - Avoid abbreviations except for trivial loop counters.

2. **No Magic Numbers**
   - Replace numeric literals with named constants (e.g., `MAX_TASKS`, `KERNEL_STACK_SIZE`).
   - Use `#define` or `const` for all limits and buffer sizes.

3. **Static Functions for Internal Helpers**
   - Declare internal helper functions as `static` to limit scope.
   - Only expose API functions in headers.

4. **Modular Functions**
   - Divide complex logic into small, focused functions.
   - Each function should have a single responsibility (e.g., `init_scheduler()`, `handle_syscall()`).

5. **Statement Bodies on Separate Lines**
   - Always put control flow bodies (if, while, for) on separate lines for readability.

6. **All Variables Must Be Initialized**
   - Never leave variables uninitialized. Always provide explicit initial values.

**Rationale:**

These standards ensure code is:
- **Reliable**: Reduces bugs in kernel and drivers
- **Maintainable**: Easy to update and extend
- **Consistent**: All modules follow the same style
- **Debuggable**: Issues are easier to identify and fix



Conclusion
----------

High code quality is critical for ThunderOS. Following these standards ensures the project remains reliable, maintainable, and easy to extend as it grows.