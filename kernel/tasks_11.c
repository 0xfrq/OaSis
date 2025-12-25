#include "tasks_demo.h"
#include "syscall.h"
#include <stddef.h>

// Day 11 task: Test block device I/O
void task_block_test(void) {
    const char *msg = "[BLOCK_TEST] Starting block device test\n";
    sys_write(msg, 42);

    // Test data to write
    uint8_t test_data[512];
    for (int i = 0; i < 512; i++) {
        test_data[i] = (uint8_t)(i % 256);
    }

    // Write to block 1
    const char *write_msg = "[BLOCK_TEST] Writing 512 bytes to block 1\n";
    sys_write(write_msg, 44);

    int result = sys_block_write(1, test_data);
    if (result == 0) {
        const char *success_msg = "[BLOCK_TEST] Write successful\n";
        sys_write(success_msg, 29);
    } else {
        const char *fail_msg = "[BLOCK_TEST] Write failed\n";
        sys_write(fail_msg, 25);
    }

    // Read from block 1
    uint8_t read_data[512];
    const char *read_msg = "[BLOCK_TEST] Reading 512 bytes from block 1\n";
    sys_write(read_msg, 45);

    result = sys_block_read(1, read_data);
    if (result == 0) {
        const char *success_msg = "[BLOCK_TEST] Read successful\n";
        sys_write(success_msg, 28);

        // Verify data
        int verified = 1;
        for (int i = 0; i < 512; i++) {
            if (read_data[i] != test_data[i]) {
                verified = 0;
                break;
            }
        }

        if (verified) {
            const char *verify_msg = "[BLOCK_TEST] Data verification passed\n";
            sys_write(verify_msg, 36);
        } else {
            const char *verify_msg = "[BLOCK_TEST] Data verification failed\n";
            sys_write(verify_msg, 36);
        }
    } else {
        const char *fail_msg = "[BLOCK_TEST] Read failed\n";
        sys_write(fail_msg, 24);
    }

    // Flush cache
    const char *flush_msg = "[BLOCK_TEST] Flushing block cache\n";
    sys_write(flush_msg, 33);
    sys_block_flush();

    const char *done_msg = "[BLOCK_TEST] Block device test completed\n";
    sys_write(done_msg, 40);

    // Exit
    sys_exit(0);
}