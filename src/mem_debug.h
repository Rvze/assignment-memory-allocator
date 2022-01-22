//
// Created by Ньургун Макаров on 22.01.2022.
//

#ifndef ASSIGNMENT_MEMORY_ALLOCATOR_MEM_DEBUG_H
#define ASSIGNMENT_MEMORY_ALLOCATOR_MEM_DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include "mem_internals.h"
#include "mem.h"

void debug_struct_info(FILE *f, void const *addr);

void debug_heap(FILE *f, void const *ptr);

void debug_block(struct block_header *b, const char *fmt, ...);

void debug(const char *fmt, ...);

#endif //ASSIGNMENT_MEMORY_ALLOCATOR_MEM_DEBUG_H
