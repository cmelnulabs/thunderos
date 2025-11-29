.. _internals-virtual-terminals:

Virtual Terminals (vterm)
=========================

ThunderOS provides a virtual terminal subsystem that enables multiple independent terminal sessions on a single physical display. This allows running different programs on separate terminals and switching between them.

Overview
--------

The virtual terminal system (``vterm``) provides:

- **6 virtual terminals** (VT1-VT6) with independent screen buffers
- **Terminal switching** via ESC+1 through ESC+6
- **Per-terminal input buffers** for isolated input routing
- **Per-process controlling terminal** (tty) assignment
- **Dual-mode operation**: GPU framebuffer or UART-only fallback

Architecture
------------

Component Layout
~~~~~~~~~~~~~~~~

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────────┐
    │                     Virtual Terminal System                      │
    ├─────────────────────────────────────────────────────────────────┤
    │                                                                  │
    │  ┌──────────┐ ┌──────────┐ ┌──────────┐     ┌──────────┐       │
    │  │   VT1    │ │   VT2    │ │   VT3    │ ... │   VT6    │       │
    │  │ (shell)  │ │ (shell)  │ │ (empty)  │     │ (empty)  │       │
    │  │  PID 1   │ │  PID 2   │ │          │     │          │       │
    │  ├──────────┤ ├──────────┤ ├──────────┤     ├──────────┤       │
    │  │ Screen   │ │ Screen   │ │ Screen   │     │ Screen   │       │
    │  │ Buffer   │ │ Buffer   │ │ Buffer   │     │ Buffer   │       │
    │  │ 80x24    │ │ 80x24    │ │ 80x24    │     │ 80x24    │       │
    │  ├──────────┤ ├──────────┤ ├──────────┤     ├──────────┤       │
    │  │ Input    │ │ Input    │ │ Input    │     │ Input    │       │
    │  │ Buffer   │ │ Buffer   │ │ Buffer   │     │ Buffer   │       │
    │  └──────────┘ └──────────┘ └──────────┘     └──────────┘       │
    │                      │                                          │
    │              ┌───────▼───────┐                                  │
    │              │ Active Term   │ ← g_active_terminal              │
    │              │ (VT1 or VT2)  │                                  │
    │              └───────┬───────┘                                  │
    │                      │                                          │
    │         ┌────────────┴────────────┐                             │
    │         ▼                         ▼                             │
    │  ┌─────────────┐          ┌─────────────┐                       │
    │  │ Framebuffer │          │    UART     │                       │
    │  │ (if GPU)    │          │ (fallback)  │                       │
    │  └─────────────┘          └─────────────┘                       │
    └─────────────────────────────────────────────────────────────────┘

Data Structures
---------------

Terminal Structure
~~~~~~~~~~~~~~~~~~

Each virtual terminal maintains its own state:

.. code-block:: c

    typedef struct {
        /* Screen buffer - character + attributes per cell */
        vterm_cell_t buffer[VTERM_MAX_ROWS][VTERM_MAX_COLS];
        
        /* Cursor position */
        uint32_t cursor_col;
        uint32_t cursor_row;
        int cursor_visible;
        
        /* Terminal dimensions */
        uint32_t cols;
        uint32_t rows;
        
        /* Current text attributes */
        uint8_t fg_color;        /* Foreground (ANSI 0-15) */
        uint8_t bg_color;        /* Background (ANSI 0-15) */
        uint8_t attrs;           /* Bold, underline, etc. */
        
        /* Terminal metadata */
        char name[16];           /* "VT1", "VT2", etc. */
        int active;              /* Is this the active terminal? */
    } vterm_t;

Input Buffer
~~~~~~~~~~~~

Each terminal has a circular input buffer:

.. code-block:: c

    #define INPUT_BUFFER_SIZE 64

    typedef struct {
        char buffer[INPUT_BUFFER_SIZE];
        int head;                /* Write position */
        int tail;                /* Read position */
    } vterm_input_buffer_t;

    static vterm_input_buffer_t g_input_buffers[VTERM_MAX_TERMINALS];

Cell Structure
~~~~~~~~~~~~~~

.. code-block:: c

    typedef struct {
        char ch;                 /* Character */
        uint8_t fg_color;        /* Foreground color (ANSI) */
        uint8_t bg_color;        /* Background color (ANSI) */
        uint8_t attrs;           /* Attributes (bold, etc.) */
    } vterm_cell_t;

Terminal Switching
------------------

Escape Sequences
~~~~~~~~~~~~~~~~

Terminal switching is triggered by escape sequences:

.. list-table:: Switch Key Sequences
   :header-rows: 1
   :widths: 20 30 50

   * - Keys
     - Sequence
     - Action
   * - ESC + 1
     - ``0x1B 0x31``
     - Switch to VT1
   * - ESC + 2
     - ``0x1B 0x32``
     - Switch to VT2
   * - ESC + 3
     - ``0x1B 0x33``
     - Switch to VT3
   * - ESC + 4-6
     - ``0x1B 0x34-36``
     - Switch to VT4-6

Switch Process
~~~~~~~~~~~~~~

.. code-block:: c

    int vterm_switch(int index) {
        if (index == g_active_terminal)
            return 0;  /* Already active */
        
        /* Update active terminal */
        g_active_terminal = index;
        g_input_terminal = index;
        
        /* Refresh display from new terminal's buffer */
        vterm_refresh();
        vterm_draw_status_bar();
        vterm_flush();
        
        return 0;
    }

When switching terminals:

1. The previous terminal's screen buffer is preserved
2. The new terminal's buffer is rendered to display
3. Input routing switches to the new terminal
4. Processes on both terminals continue running

Input Handling
--------------

Input Flow
~~~~~~~~~~

.. code-block:: text

    UART Input
        │
        ▼
    ┌─────────────────────┐
    │  Timer Interrupt    │  (every 100ms)
    │  vterm_poll_input() │
    └─────────┬───────────┘
              │
              ▼
    ┌─────────────────────┐
    │ vterm_process_input │
    │                     │
    │ ESC+n? → switch VT  │
    │ else  → buffer char │
    └─────────┬───────────┘
              │
              ▼
    ┌─────────────────────┐
    │ input_buffer_put_to │
    │ (active terminal)   │
    └─────────────────────┘

Hybrid Input Model
~~~~~~~~~~~~~~~~~~

The ``sys_read`` syscall uses a hybrid approach for responsive input:

.. code-block:: c

    /* In sys_read for stdin */
    while (1) {
        /* Check terminal's input buffer first */
        if (vterm_has_buffered_input_for(tty)) {
            return vterm_get_buffered_input_for(tty);
        }
        
        /* If active terminal, also check UART directly */
        if (tty == vterm_get_active_index() && 
            hal_uart_data_available()) {
            char c = hal_uart_getc_nonblock();
            c = vterm_process_input(c);
            if (c != 0) return c;
        }
        
        /* Yield and retry */
        process_yield();
    }

This ensures:

- Immediate response for the active terminal (direct UART read)
- Background terminals receive input via timer polling
- No busy-waiting (yields when no input available)

Process-Terminal Binding
------------------------

Controlling Terminal
~~~~~~~~~~~~~~~~~~~~

Each process has a controlling terminal (tty):

.. code-block:: c

    struct process {
        /* ... other fields ... */
        int controlling_tty;     /* Terminal index (0-5) or -1 */
    };

Setting the TTY
~~~~~~~~~~~~~~~

.. code-block:: c

    /* Set process's controlling terminal */
    void process_set_tty(struct process *proc, int tty);
    
    /* Get process's controlling terminal */
    int process_get_tty(struct process *proc);

At boot, the kernel spawns shells on VT1 and VT2:

.. code-block:: c

    /* In kernel main.c */
    pid_t shell1 = spawn_user_shell("/bin/ush");
    process_set_tty(process_get(shell1), 0);  /* VT1 */
    
    pid_t shell2 = spawn_user_shell("/bin/ush");
    process_set_tty(process_get(shell2), 1);  /* VT2 */

Output Routing
--------------

Per-Terminal Output
~~~~~~~~~~~~~~~~~~~

Write operations route to the process's controlling terminal:

.. code-block:: c

    /* In sys_write for stdout/stderr */
    if (vterm_available()) {
        int tty = process_get_tty(proc);
        if (tty >= 0) {
            /* Write to process's terminal buffer */
            for (size_t i = 0; i < count; i++) {
                vterm_putc_to(tty, buffer[i]);
            }
            /* Only flush if active terminal */
            if (tty == vterm_get_active_index()) {
                vterm_flush();
            }
        }
    }

This allows:

- Background processes to continue writing to their terminal
- Output accumulates in terminal buffer
- Visible when switching to that terminal

API Reference
-------------

Initialization
~~~~~~~~~~~~~~

.. code-block:: c

    /* Initialize virtual terminal subsystem */
    int vterm_init(void);
    
    /* Check if vterm is available */
    int vterm_available(void);

Terminal Operations
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /* Switch to terminal by index (0-5) */
    int vterm_switch(int index);
    
    /* Get active terminal index */
    int vterm_get_active_index(void);
    
    /* Write character to specific terminal */
    void vterm_putc_to(int index, char c);
    
    /* Write character to active terminal */
    void vterm_putc(char c);
    
    /* Flush display (render buffer to screen) */
    void vterm_flush(void);

Input Functions
~~~~~~~~~~~~~~~

.. code-block:: c

    /* Process input character (handles escape sequences) */
    char vterm_process_input(char c);
    
    /* Poll UART and buffer to active terminal */
    int vterm_poll_input(void);
    
    /* Check/get buffered input for specific terminal */
    int vterm_has_buffered_input_for(int index);
    int vterm_get_buffered_input_for(int index);

Display Modes
-------------

GPU Mode (with VirtIO GPU)
~~~~~~~~~~~~~~~~~~~~~~~~~~

When a VirtIO GPU is available:

- Full 80x24 character display with colors
- Status bar showing active terminal
- Cursor rendering

UART-Only Mode
~~~~~~~~~~~~~~

When no GPU is detected:

- Output goes to serial console
- Terminal switching still works (via escape sequences)
- Per-terminal buffers maintained (for future GPU hotplug)

Files
-----

.. code-block:: text

    kernel/drivers/vterm.c          # Virtual terminal implementation
    include/drivers/vterm.h         # Public API
    kernel/arch/riscv64/drivers/timer.c  # Input polling in timer ISR

See Also
--------

- :doc:`shell` - User-mode shell that runs on virtual terminals
- :doc:`process_management` - Process TTY assignment
- :doc:`syscalls` - sys_read/sys_write terminal routing
