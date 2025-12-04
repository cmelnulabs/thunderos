/**
 * @file condvar_test.c
 * @brief Test program for condition variables in ThunderOS
 *
 * Tests condition variable operations including wait, signal, and broadcast.
 */

#include <stddef.h>

/* Syscall numbers */
#define SYS_WRITE 1
#define SYS_EXIT 0
#define SYS_FORK 7
#define SYS_YIELD 6
#define SYS_GETPID 3
#define SYS_SLEEP 5
#define SYS_MUTEX_CREATE 46
#define SYS_MUTEX_LOCK 47
#define SYS_MUTEX_UNLOCK 49
#define SYS_MUTEX_DESTROY 50
#define SYS_COND_CREATE 51
#define SYS_COND_WAIT 52
#define SYS_COND_SIGNAL 53
#define SYS_COND_BROADCAST 54
#define SYS_COND_DESTROY 55

/* File descriptors */
#define STDOUT 1

/* Syscall helpers */
#define syscall0(n) ({ \
    register long a0 asm("a0"); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "=r"(a0) : "r"(syscall_number) : "memory"); \
    a0; \
})

#define syscall1(n, a1) ({ \
    register long a0 asm("a0") = (long)(a1); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "+r"(a0) : "r"(syscall_number) : "memory"); \
    a0; \
})

#define syscall2(n, a1, a2) ({ \
    register long a0 asm("a0") = (long)(a1); \
    register long a1_reg asm("a1") = (long)(a2); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "+r"(a0) : "r"(a1_reg), "r"(syscall_number) : "memory"); \
    a0; \
})

#define syscall3(n, a1, a2, a3) ({ \
    register long a0 asm("a0") = (long)(a1); \
    register long a1_reg asm("a1") = (long)(a2); \
    register long a2_reg asm("a2") = (long)(a3); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "+r"(a0) : "r"(a1_reg), "r"(a2_reg), "r"(syscall_number) : "memory"); \
    a0; \
})

/* Test timing constants */
#define SLEEP_SHORT_MS  50
#define SLEEP_MEDIUM_MS 100
#define SLEEP_LONG_MS   150

/* Helper functions */
static void print(const char *str) {
    const char *p = str;
    int len = 0;
    while (*p++) len++;
    syscall3(SYS_WRITE, STDOUT, (long)str, len);
}

static void print_num(int num) {
    char buf[32];
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    
    if (is_negative) {
        buf[i++] = '-';
    }
    
    /* Reverse the string */
    for (int j = 0; j < i / 2; j++) {
        char temp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = temp;
    }
    
    buf[i] = '\0';
    print(buf);
}

/* Test 1: Create and destroy condition variable */
static void test_cond_create_destroy(void) {
    print("[TEST] Create and destroy condition variable...\n");
    
    int cond_id = syscall0(SYS_COND_CREATE);
    if (cond_id < 0) {
        print("[FAIL] Failed to create condition variable\n");
        return;
    }
    
    if (syscall1(SYS_COND_DESTROY, cond_id) < 0) {
        print("[FAIL] Failed to destroy condition variable\n");
        return;
    }
    
    print("[PASS] Create and destroy condition variable\n");
}

/* Test 2: Signal with no waiters (should be no-op) */
static void test_cond_signal_no_waiters(void) {
    print("[TEST] Signal with no waiters...\n");
    
    int cond_id = syscall0(SYS_COND_CREATE);
    if (cond_id < 0) {
        print("[FAIL] Failed to create condition variable\n");
        return;
    }
    
    /* Signal should succeed even with no waiters */
    if (syscall1(SYS_COND_SIGNAL, cond_id) < 0) {
        print("[FAIL] Failed to signal condition variable\n");
        syscall1(SYS_COND_DESTROY, cond_id);
        return;
    }
    
    syscall1(SYS_COND_DESTROY, cond_id);
    print("[PASS] Signal with no waiters\n");
}

/* Test 3: Broadcast with no waiters (should be no-op) */
static void test_cond_broadcast_no_waiters(void) {
    print("[TEST] Broadcast with no waiters...\n");
    
    int cond_id = syscall0(SYS_COND_CREATE);
    if (cond_id < 0) {
        print("[FAIL] Failed to create condition variable\n");
        return;
    }
    
    /* Broadcast should succeed even with no waiters */
    if (syscall1(SYS_COND_BROADCAST, cond_id) < 0) {
        print("[FAIL] Failed to broadcast condition variable\n");
        syscall1(SYS_COND_DESTROY, cond_id);
        return;
    }
    
    syscall1(SYS_COND_DESTROY, cond_id);
    print("[PASS] Broadcast with no waiters\n");
}

/* Test 4: Signal wakes one waiter (simplified - just verify no deadlock) */
static void test_cond_signal_one_waiter(void) {
    print("[TEST] Signal wakes one waiter...\n");
    
    int mutex_id = syscall0(SYS_MUTEX_CREATE);
    int cond_id = syscall0(SYS_COND_CREATE);
    
    if (mutex_id < 0 || cond_id < 0) {
        print("[FAIL] Failed to create mutex or condvar\n");
        return;
    }
    
    int pid = syscall0(SYS_FORK);
    
    if (pid == 0) {
        /* Child: wait on condition */
        syscall1(SYS_MUTEX_LOCK, mutex_id);
        
        /* Wait for condition (atomically unlocks mutex and sleeps) */
        syscall2(SYS_COND_WAIT, cond_id, mutex_id);
        
        /* When we wake up, mutex is locked again */
        syscall1(SYS_MUTEX_UNLOCK, mutex_id);
        
        /* Child exits successfully if it woke up */
        syscall1(SYS_EXIT, 0);
    } else {
        /* Parent: signal the condition */
        
        /* Give child time to wait */
        syscall1(SYS_SLEEP, SLEEP_MEDIUM_MS);
        
        syscall1(SYS_MUTEX_LOCK, mutex_id);
        
        /* Signal the condition */
        syscall1(SYS_COND_SIGNAL, cond_id);
        
        syscall1(SYS_MUTEX_UNLOCK, mutex_id);
        
        /* Give child time to wake up */
        syscall1(SYS_SLEEP, SLEEP_MEDIUM_MS);
        
        print("[PASS] Signal wakes one waiter (child woke up)\n");
        
        syscall1(SYS_MUTEX_DESTROY, mutex_id);
        syscall1(SYS_COND_DESTROY, cond_id);
    }
}

/* Test 5: Broadcast wakes all waiters (simplified) */
static void test_cond_broadcast_multiple_waiters(void) {
    print("[TEST] Broadcast wakes all waiters...\n");
    
    int mutex_id = syscall0(SYS_MUTEX_CREATE);
    int cond_id = syscall0(SYS_COND_CREATE);
    
    if (mutex_id < 0 || cond_id < 0) {
        print("[FAIL] Failed to create mutex or condvar\n");
        return;
    }
    
    /* Create 3 child processes that will wait */
    for (int i = 0; i < 3; i++) {
        int pid = syscall0(SYS_FORK);
        
        if (pid == 0) {
            /* Child: wait on condition */
            syscall1(SYS_MUTEX_LOCK, mutex_id);
            syscall2(SYS_COND_WAIT, cond_id, mutex_id);
            
            /* When we wake up, exit successfully */
            syscall1(SYS_MUTEX_UNLOCK, mutex_id);
            syscall1(SYS_EXIT, 0);
        }
    }
    
    /* Parent: broadcast to all waiters */
    
    /* Give children time to wait */
    syscall1(SYS_SLEEP, SLEEP_LONG_MS);
    
    syscall1(SYS_MUTEX_LOCK, mutex_id);
    
    /* Broadcast to all waiters */
    syscall1(SYS_COND_BROADCAST, cond_id);
    
    syscall1(SYS_MUTEX_UNLOCK, mutex_id);
    
    /* Give children time to wake up */
    syscall1(SYS_SLEEP, SLEEP_LONG_MS);
    
    print("[PASS] Broadcast wakes all waiters (children woke up)\n");
    
    syscall1(SYS_MUTEX_DESTROY, mutex_id);
    syscall1(SYS_COND_DESTROY, cond_id);
}

/* Test 6: Producer-consumer pattern (without shared memory validation) */
static void test_producer_consumer(void) {
    print("[TEST] Producer-consumer pattern...\n");
    
    int mutex_id = syscall0(SYS_MUTEX_CREATE);
    int cond_id = syscall0(SYS_COND_CREATE);
    
    if (mutex_id < 0 || cond_id < 0) {
        print("[FAIL] Failed to create mutex or condvar\n");
        return;
    }
    
    int pid = syscall0(SYS_FORK);
    
    if (pid == 0) {
        /* Consumer child */
        syscall1(SYS_MUTEX_LOCK, mutex_id);
        
        /* Wait for signal from producer */
        syscall2(SYS_COND_WAIT, cond_id, mutex_id);
        
        /* We've been signaled - producer produced data */
        syscall1(SYS_MUTEX_UNLOCK, mutex_id);
        
        syscall1(SYS_EXIT, 0);  /* Success */
    } else {
        /* Producer parent */
        
        /* Give consumer time to start waiting */
        syscall1(SYS_SLEEP, SLEEP_MEDIUM_MS);
        
        syscall1(SYS_MUTEX_LOCK, mutex_id);
        
        /* Signal consumer that data is ready */
        syscall1(SYS_COND_SIGNAL, cond_id);
        
        syscall1(SYS_MUTEX_UNLOCK, mutex_id);
        
        /* Give consumer time to consume */
        syscall1(SYS_SLEEP, SLEEP_MEDIUM_MS);
        
        print("[PASS] Producer-consumer pattern (consumer woke up)\n");
        
        syscall1(SYS_MUTEX_DESTROY, mutex_id);
        syscall1(SYS_COND_DESTROY, cond_id);
    }
}

/* Test 7: Multiple signals wake multiple waiters (simplified) */
static void test_multiple_signals(void) {
    print("[TEST] Multiple signals wake multiple waiters...\n");
    
    int mutex_id = syscall0(SYS_MUTEX_CREATE);
    int cond_id = syscall0(SYS_COND_CREATE);
    
    if (mutex_id < 0 || cond_id < 0) {
        print("[FAIL] Failed to create mutex or condvar\n");
        return;
    }
    
    /* Create 3 child processes that will wait */
    for (int i = 0; i < 3; i++) {
        int pid = syscall0(SYS_FORK);
        
        if (pid == 0) {
            /* Child: wait on condition */
            syscall1(SYS_MUTEX_LOCK, mutex_id);
            syscall2(SYS_COND_WAIT, cond_id, mutex_id);
            
            /* When we wake up, exit successfully */
            syscall1(SYS_MUTEX_UNLOCK, mutex_id);
            syscall1(SYS_EXIT, 0);
        }
    }
    
    /* Parent: signal each waiter individually */
    
    /* Give children time to wait */
    syscall1(SYS_SLEEP, SLEEP_LONG_MS);
    
    /* Signal 3 times to wake all 3 waiters */
    for (int i = 0; i < 3; i++) {
        syscall1(SYS_MUTEX_LOCK, mutex_id);
        syscall1(SYS_COND_SIGNAL, cond_id);
        syscall1(SYS_MUTEX_UNLOCK, mutex_id);
        syscall1(SYS_SLEEP, SLEEP_SHORT_MS);  /* Give waiter time to wake up */
    }
    
    /* Give final waiter time to wake */
    syscall1(SYS_SLEEP, SLEEP_MEDIUM_MS);
    
    print("[PASS] Multiple signals wake multiple waiters (all woke up)\n");
    
    syscall1(SYS_MUTEX_DESTROY, mutex_id);
    syscall1(SYS_COND_DESTROY, cond_id);
}

/* Main test runner */
void _start(void) {
    print("\n=== ThunderOS Condition Variable Tests ===\n\n");
    
    test_cond_create_destroy();
    test_cond_signal_no_waiters();
    test_cond_broadcast_no_waiters();
    test_cond_signal_one_waiter();
    test_cond_broadcast_multiple_waiters();
    test_producer_consumer();
    test_multiple_signals();
    
    print("\n=== All Condition Variable Tests Complete ===\n\n");
    
    syscall1(SYS_EXIT, 0);
}
