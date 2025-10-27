Memory Model
============

RISC-V uses a weakly-ordered memory model (RVWMO) and supports virtual memory via page tables.

Weak Memory Ordering
--------------------

Loads and stores may be reordered by hardware for performance. Use ``fence`` instructions for ordering guarantees.

Virtual Memory
--------------

S-mode manages virtual memory using the ``satp`` CSR and page tables.

Paging Modes (RV64)
~~~~~~~~~~~~~~~~~~~

* **Bare**: No translation (physical addresses)
* **Sv39**: 39-bit virtual address space (3-level page table)
* **Sv48**: 48-bit virtual address space (4-level page table)
* **Sv57**: 57-bit virtual address space (5-level page table)

ThunderOS will use **Sv39** (future implementation).

See Also
--------

* :doc:`../internals/memory_layout` - Physical memory layout
* Official RISC-V Privileged Specification - Virtual Memory chapter
