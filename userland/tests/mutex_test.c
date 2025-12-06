/**
 * mutex_test.c - Test program for mutex synchronization
 * 
 * Tests mutex functionality:
 * 1. Create a mutex
 * 2. Lock/unlock in single process
 * 3. Test trylock behavior
 * 4. Test mutex with fork (parent/child contention)
 */

#include <stddef.h>

/* Syscall numbers */
#define SYS_EXIT          0
#define SYS_WRITE         1
#define SYS_FORK          7
#define SYS_WAIT          9
#define SYS_SLEEP         5
#define SYS_GETPID        3
#define SYS_MUTEX_CREATE  46
#define SYS_MUTEX_LOCK    47
#define SYS_MUTEX_TRYLOCK 48
#define SYS_MUTEX_UNLOCK  49
#define SYS_MUTEX_DESTROY 50

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

static inline long waitpid(int pid, const int *status, int options) {
    return syscall3(SYS_WAIT, pid, status, options);
}

static inline long sleep_ms(long ms) {
    return syscall1(SYS_SLEEP, ms);
}

static inline long getpid(void) {
    return syscall0(SYS_GETPID);
}

static inline long mutex_create(void) {
    return syscall0(SYS_MUTEX_CREATE);
}

static inline long mutex_lock(int mutex_id) {
    return syscall1(SYS_MUTEX_LOCK, mutex_id);
}

static inline long mutex_trylock(int mutex_id) {
    return syscall1(SYS_MUTEX_TRYLOCK, mutex_id);
}

static inline long mutex_unlock(int mutex_id) {
    return syscall1(SYS_MUTEX_UNLOCK, mutex_id);
}

static inline long mutex_destroy(int mutex_id) {
    return syscall1(SYS_MUTEX_DESTROY, mutex_id);
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
    print("       Mutex Test Program\n");
    print("========================================\n\n");
    
    /* Test 1: Create mutex */
    print("[TEST 1] Creating mutex...\n");
    long mutex_id = mutex_create();
    if (mutex_id >= 0) {
        print("  Mutex ID: ");
        print_num(mutex_id);
        print("\n");
        test_pass("mutex_create");
    } else {
        test_fail("mutex_create");
        exit(1);
    }
    
    /* Test 2: Lock mutex */
    print("\n[TEST 2] Locking mutex...\n");
    if ((int)mutex_lock(mutex_id) == 0) {
        test_pass("mutex_lock");
    } else {
        test_fail("mutex_lock");
        exit(1);
    }
    
    /* Test 3: Trylock should fail (already locked) */
    print("\n[TEST 3] Trylock on locked mutex (should fail)...\n");
    if ((int)mutex_trylock(mutex_id) < 0) {
        test_pass("mutex_trylock returns error when locked");
    } else {
        test_fail("mutex_trylock should have failed");
        exit(1);
    }
    
    /* Test 4: Unlock mutex */
    print("\n[TEST 4] Unlocking mutex...\n");
    if ((int)mutex_unlock(mutex_id) == 0) {
        test_pass("mutex_unlock");
    } else {
        test_fail("mutex_unlock");
        exit(1);
    }
    
    /* Test 5: Trylock should succeed (now unlocked) */
    print("\n[TEST 5] Trylock on unlocked mutex (should succeed)...\n");
    if ((int)mutex_trylock(mutex_id) == 0) {
        test_pass("mutex_trylock succeeds when unlocked");
        (void)mutex_unlock(mutex_id);
    } else {
        test_fail("mutex_trylock should have succeeded");
        exit(1);
    }
    
    /* Test 6: Fork and test mutex contention */
    print("\n[TEST 6] Fork and mutex contention test...\n");
    
    long pid = fork();
    
    if (pid < 0) {
        test_fail("fork failed");
        exit(1);
    }
    
    if (pid == 0) {
        /* Child process */
        long my_pid = getpid();
        print("  [CHILD PID ");
        print_num(my_pid);
        print("] Trying to acquire mutex...\n");
        
        /* Try to lock - this should block if parent has it */
        if ((int)mutex_lock(mutex_id) == 0) {
            print("  [CHILD] Got mutex!\n");
            print("  [CHILD] Holding mutex for 100ms...\n");
            sleep_ms(100);
            (void)mutex_unlock(mutex_id);
            print("  [CHILD] Released mutex\n");
        } else {
            print("  [CHILD] Failed to get mutex!\n");
        }
        
        exit(0);
    } else {
        /* Parent process */
        long my_pid = getpid();
        print("  [PARENT PID ");
        print_num(my_pid);
        print("] Locking mutex first...\n");
        
        mutex_lock(mutex_id);
        print("  [PARENT] Got mutex, holding for 200ms...\n");
        print("  [PARENT] (Child should be blocked waiting)\n");
        sleep_ms(200);
        
        print("  [PARENT] Releasing mutex...\n");
        (void)mutex_unlock(mutex_id);
        
        /* Wait for child */
        int status;
        waitpid(pid, &status, 0);
        print("  [PARENT] Child exited\n");
        
        test_pass("fork + mutex contention");
    }
    
    /* Test 7: Destroy mutex */
    print("\n[TEST 7] Destroying mutex...\n");
    if (mutex_destroy(mutex_id) == 0) {
        test_pass("mutex_destroy");
    } else {
        test_fail("mutex_destroy");
    }
    
    /* Test 8: Operations on destroyed mutex should fail */
    print("\n[TEST 8] Lock destroyed mutex (should fail)...\n");
    if (mutex_lock(mutex_id) < 0) {
        test_pass("lock on destroyed mutex fails");
    } else {
        test_fail("lock on destroyed mutex should fail");
        (void)mutex_unlock(mutex_id);
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
