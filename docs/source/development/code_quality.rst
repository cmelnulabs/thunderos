Code Quality, Style & Refactoring
=================================

This document consolidates coding conventions, style rules and quality guidelines for ThunderOS. It is the canonical reference for contributors.

Code Style Standards
--------------------

**C Code:**

* Use K&R style bracing
* 4 spaces indentation (no tabs)
* Descriptive variable names
* Comment non-obvious code

.. code-block:: c

   // Good
   void init_memory_manager(void) {
       size_t total_pages = calculate_available_pages();
       for (size_t i = 0; i < total_pages; i++) {
           mark_page_free(i);
       }
   }

   // Bad
   void imm(){
   int tp=cap();for(int i=0;i<tp;i++){mpf(i);}}

**Assembly:**

* Comment each logical block
* Use meaningful labels
* Align code for readability

.. code-block:: asm

   # Good
   clear_bss:
       beq t0, t1, clear_bss_done    # Exit if done
       sd zero, 0(t0)                # Zero current word
       addi t0, t0, 8                # Advance pointer
       j clear_bss                   # Loop
   clear_bss_done:

Mandatory Rules
---------------

1. **Descriptive Names**
   - Use full, descriptive names that indicate purpose (e.g., `process_id`, `interrupt_vector`, `task_struct`).
   - Avoid abbreviations and single-letter names except for trivial loop counters.

2. **No Magic Numbers**
   - Replace numeric literals with named constants (e.g., `MAX_TASKS`, `KERNEL_STACK_SIZE`).
   - Use `#define` or `const` for limits, buffer sizes and hardware-specific values.

3. **Limit Symbol Scope**
   - Declare internal helper functions `static` to limit scope to the translation unit.
   - Only expose public APIs via headers.

4. **Small, Single-Purpose Functions**
   - Break complex logic into small functions. Each function should have one clear responsibility (e.g., `init_scheduler()`, `handle_syscall()`).

5. **Clear Control Flow Formatting**
   - Always put control flow bodies (if, while, for) on separate lines from their conditions for readability.
   - Use K&R style bracing consistently.

6. **Initialize Variables**
   - Always explicitly initialize variables (e.g., `int idx = 0;`, `void *ptr = NULL;`).

7. **Forward Declarations & File Organization**
   - Place constants and includes at the top of the file.
   - Add forward declarations for helper functions immediately after constants.
   - Implement helper functions before public API functions.

Naming Conventions
------------------

**Functions:**
   * ``lowercase_with_underscores()``
   * Use suffixes like ``_handler``, ``_count``, ``_buffer`` where helpful

**Macros/Constants:**
   * ``UPPERCASE_WITH_UNDERSCORES``
   * Replace numeric literals with named constants

**Types:**
   * ``PascalCase`` or ``lowercase_t``

**Variables:**
   * Prefer ``identifier_buffer``, ``index_expression``, ``field_index`` over single-letter names
   * Use full, descriptive names

Recommended Practices
---------------------

- **Buffer and index names**: prefer descriptive names over single-letter abbreviations.
- **Error handling**: adopt consistent error codes and message formatting across modules.
- **Comments**: prefer short explanatory comments for non-obvious behavior; rely on self-documenting names where possible. Comment each logical block in assembly.
- **Assembly style**: label jump targets clearly and prefer pseudo-instructions when clear

Refactoring Principles
----------------------

- **Single Responsibility**: prefer one responsibility per function/module.
- **Reduce Complexity**: minimize nesting, extract helpers, and use early returns.
- **Self-Documenting Code**: choose names and structure that reduce the need for long comments.
- **Encapsulation**: move global state and symbol table logic into well-defined modules.