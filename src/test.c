//
// Created by Ньургун Макаров on 22.01.2022.
//

#include "test.h"
#include "mem_debug.h"
#include "util.h"
#include "mem_internals.h"

#define newline "\n"
#define  TEST_OK(x, y) "TEST " x " is passed: " y newline
#define TEST_BAD(msg1, msg2) "TEST " msg1 " is failed: " msg2 newline

void *heap;
struct block_header *header;

bool prepare_tests() {
    heap = heap_init(2000);
    header = (struct block_header *) heap;
    if (heap == NULL || header == NULL)
        return false;
    return true;
}

struct test_result run_test_1(struct block_header *heap) {
    debug(" ---- TEST 1 ---- ""\n");
    debug_heap(stdout, header);
    void *test_array = _malloc(1000, heap);
    debug_heap(stdout, header);
    if (test_array == NULL)
        return (struct test_result) {.success = false, .message = TEST_BAD("№1", "can't allocate memory")};

    if (!heap->is_free)
        return (struct test_result) {.success = false, .message = TEST_BAD("№1", "wrong allocation")};

    if (heap->capacity.bytes != 1000)
        return (struct test_result) {.success = false, .message = TEST_BAD("№1",
                                                                           "capacity does not equal the expected")};
    debug_heap(stdout, header);
    _free(test_array);
    return (struct test_result) {.success = true, .message = TEST_OK("№1", "")};

}

void run_test() {
    if (!prepare_tests()) {
        err("Preparing error!");
    }
    run_test_1(heap);
}
