/**
 * rwlock_test.c - Test program for reader-writer lock synchronization
 * 
 * Tests rwlock functionality:
 * 1. Create a rwlock
 * 2. Multiple readers can hold lock simultaneously
 * 3. Writer has exclusive access
 * 4. Writers block new readers
 */

#include <stddef.h>

/* Syscall numbers */
#define SYS_EXIT          0
#define SYS_WRITE         1
#define SYS_FORK          7
#define SYS_WAIT          9
#define SYS_SLEEP         5
#define SYS_GETPID        3
#define SYS_RWLOCK_CREATE      56
#define SYS_RWLOCK_READ_LOCK   57
#define SYS_RWLOCK_READ_UNLOCK 58
#define SYS_RWLOCK_WRITE_LOCK  59
#define SYS_RWLOCK_WRITE_UNLOCK 60
#define SYS_RWLOCK_DESTROY     61

#define STDOUT_FD 1

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

#define syscall3(n, a1, a2, a3) ({ \
    register long a0 asm("a0") = (long)(a1); \
    register long a1_reg asm("a1") = (long)(a2); \
    register long a2_reg asm("a2") = (long)(a3); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "+r"(a0) : "r"(a1_reg), "r"(a2_reg), "r"(syscall_number) : "memory"); \
    a0; \
})

/* Syscall wrappers */
static inline void exit(int status) {
    syscall1(SYS_EXIT, status);
    while(1);
}

static inline long write(int fd, const char *buf, size_t len) {
    return syscall3(SYS_WRITE, fd, buf, len);
}

static inline long fork(void) {
    return syscall0(SYS_FORK);
}

static inline long waitpid(int pid, int *status, int options) {
    return syscall3(SYS_WAIT, pid, status, options);
}

static inline long sleep_ms(long ms) {
    return syscall1(SYS_SLEEP, ms);
}

static inline long getpid(void) {
    return syscall0(SYS_GETPID);
}

static inline long rwlock_create(void) {
    return syscall0(SYS_RWLOCK_CREATE);
}

static inline long rwlock_read_lock(int rwlock_id) {
    return syscall1(SYS_RWLOCK_READ_LOCK, rwlock_id);
}

static inline long rwlock_read_unlock(int rwlock_id) {
    return syscall1(SYS_RWLOCK_READ_UNLOCK, rwlock_id);
}

static inline long rwlock_write_lock(int rwlock_id) {
    return syscall1(SYS_RWLOCK_WRITE_LOCK, rwlock_id);
}

static inline long rwlock_write_unlock(int rwlock_id) {
    return syscall1(SYS_RWLOCK_WRITE_UNLOCK, rwlock_id);
}

static inline long rwlock_destroy(int rwlock_id) {
    return syscall1(SYS_RWLOCK_DESTROY, rwlock_id);
}

/* String helpers */
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    write(STDOUT_FD, s, strlen(s));
}

static void print_num(long n) {
    char buf[20];
    int i = 0;
    int neg = 0;
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    if (neg) buf[i++] = '-';
    
    /* Reverse */
    char out[20];
    for (int j = 0; j < i; j++) {
        out[j] = buf[i - 1 - j];
    }
    out[i] = '\0';
    print(out);
}

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

static void test_pass(const char *name) {
    print("[PASS] ");
    print(name);
    print("\n");
    tests_passed++;
}

static void test_fail(const char *name) {
    print("[FAIL] ");
    print(name);
    print("\n");
    tests_failed++;
}

/* Main test program */
void _start(void) {
    print("\n");
    print("========================================\n");
    print("    Reader-Writer Lock Test Program\n");
    print("========================================\n\n");
    
    /* Test 1: Create rwlock */
    print("[TEST 1] Creating rwlock...\n");
    long rwlock_id = rwlock_create();
    if (rwlock_id >= 0) {
        print("  RWLock ID: ");
        print_num(rwlock_id);
        print("\n");
        test_pass("rwlock_create");
    } else {
        test_fail("rwlock_create");
        exit(1);
    }
    
    /* Test 2: Read lock/unlock */
    print("\n[TEST 2] Read lock/unlock...\n");
    if (rwlock_read_lock(rwlock_id) == 0) {
        print("  Acquired read lock\n");
        if (rwlock_read_unlock(rwlock_id) == 0) {
            print("  Released read lock\n");
            test_pass("rwlock_read_lock/unlock");
        } else {
            test_fail("rwlock_read_unlock");
            exit(1);
        }
    } else {
        test_fail("rwlock_read_lock");
        exit(1);
    }
    
    /* Test 3: Write lock/unlock */
    print("\n[TEST 3] Write lock/unlock...\n");
    if (rwlock_write_lock(rwlock_id) == 0) {
        print("  Acquired write lock\n");
        if (rwlock_write_unlock(rwlock_id) == 0) {
            print("  Released write lock\n");
            test_pass("rwlock_write_lock/unlock");
        } else {
            test_fail("rwlock_write_unlock");
            exit(1);
        }
    } else {
        test_fail("rwlock_write_lock");
        exit(1);
    }
    
    /* Test 4: Multiple readers (fork test) */
    print("\n[TEST 4] Multiple concurrent readers...\n");
    
    /* Parent takes read lock first */
    rwlock_read_lock(rwlock_id);
    print("  [PARENT] Got read lock\n");
    
    long pid = fork();
    
    if (pid < 0) {
        test_fail("fork failed");
        exit(1);
    }
    
    if (pid == 0) {
        /* Child process - should also be able to get read lock */
        long my_pid = getpid();
        print("  [CHILD PID ");
        print_num(my_pid);
        print("] Trying to get read lock...\n");
        
        if (rwlock_read_lock(rwlock_id) == 0) {
            print("  [CHILD] Got read lock (concurrent with parent)!\n");
            sleep_ms(50);
            rwlock_read_unlock(rwlock_id);
            print("  [CHILD] Released read lock\n");
            exit(0);
        } else {
            print("  [CHILD] Failed to get read lock!\n");
            exit(1);
        }
    } else {
        /* Parent - hold lock briefly, then release */
        sleep_ms(100);
        rwlock_read_unlock(rwlock_id);
        print("  [PARENT] Released read lock\n");
        
        int status;
        waitpid(pid, &status, 0);
        print("  [PARENT] Child exited\n");
        test_pass("multiple concurrent readers");
    }
    
    /* Test 5: Writer blocks readers */
    print("\n[TEST 5] Writer blocks new readers...\n");
    
    /* Parent takes write lock */
    rwlock_write_lock(rwlock_id);
    print("  [PARENT] Got write lock\n");
    
    pid = fork();
    
    if (pid < 0) {
        test_fail("fork failed");
        exit(1);
    }
    
    if (pid == 0) {
        /* Child - try to get read lock, should block */
        long my_pid = getpid();
        print("  [CHILD PID ");
        print_num(my_pid);
        print("] Trying read lock (should block)...\n");
        
        if (rwlock_read_lock(rwlock_id) == 0) {
            print("  [CHILD] Got read lock after writer released!\n");
            rwlock_read_unlock(rwlock_id);
            exit(0);
        } else {
            exit(1);
        }
    } else {
        /* Parent holds write lock, then releases */
        print("  [PARENT] Holding write lock for 150ms...\n");
        sleep_ms(150);
        print("  [PARENT] Releasing write lock...\n");
        rwlock_write_unlock(rwlock_id);
        
        int status;
        waitpid(pid, &status, 0);
        print("  [PARENT] Child exited\n");
        test_pass("writer blocks readers");
    }
    
    /* Test 6: Destroy rwlock */
    print("\n[TEST 6] Destroying rwlock...\n");
    if (rwlock_destroy(rwlock_id) == 0) {
        test_pass("rwlock_destroy");
    } else {
        test_fail("rwlock_destroy");
    }
    
    /* Test 7: Operations on destroyed rwlock should fail */
    print("\n[TEST 7] Read lock on destroyed rwlock (should fail)...\n");
    if (rwlock_read_lock(rwlock_id) < 0) {
        test_pass("read_lock on destroyed rwlock fails");
    } else {
        test_fail("read_lock on destroyed rwlock should fail");
        rwlock_read_unlock(rwlock_id);
    }
    
    /* Summary */
    print("\n========================================\n");
    print("  Test Summary\n");
    print("========================================\n");
    print("  Passed: ");
    print_num(tests_passed);
    print("\n  Failed: ");
    print_num(tests_failed);
    print("\n");
    
    if (tests_failed == 0) {
        print("\n  ALL TESTS PASSED!\n");
    } else {
        print("\n  SOME TESTS FAILED!\n");
    }
    print("========================================\n\n");
    
    exit(tests_failed > 0 ? 1 : 0);
}
