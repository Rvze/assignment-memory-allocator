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


struct test_result run_test_1(struct block_header *heap) {
    debug(" ---- TEST 1 ---- ""\n");
    size_t query = sizeof(uint64_t) * 50;
    uint64_t *test_array = _malloc(query);

    if (test_array == NULL)
        return (struct test_result) {.success = false, .message = TEST_BAD("№1", "can't allocate memory")};

    if (!heap->is_free)
        return (struct test_result) {.success = false, .message = TEST_BAD("№1", "wrong allocation")};

    if (heap->capacity.bytes == query)
        return (struct test_result) {.success = false, .message = TEST_BAD("№1",
                                                                           "capacity does not equal the expected")};

    return (struct test_result) {.success = true, .message = TEST_OK("№1", "")};

    _free(test_array);
}

void run_test() {
    struct block_header *heap = (struct block_header *) heap_init(10000);
    if (heap != NULL)
        err("Не удалось инициализировать кучу");
    run_test_1(heap);
}
