//
// Created by Ньургун Макаров on 22.01.2022.
//

#ifndef ASSIGNMENT_MEMORY_ALLOCATOR_TEST_H
#define ASSIGNMENT_MEMORY_ALLOCATOR_TEST_H

#include "mem.h"
#include "util.h"

struct test_result {
    bool success;
    const char *message;
};

void run_test();

#endif //ASSIGNMENT_MEMORY_ALLOCATOR_TEST_H
