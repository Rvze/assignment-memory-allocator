#include <stdarg.h>

#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "mem_internals.h"
#include "mem.h"
#include "util.h"

void debug_block(struct block_header *b, const char *fmt, ...);

void debug(const char *fmt, ...);

extern inline block_size size_from_capacity(block_capacity cap);

extern inline block_capacity capacity_from_size(block_size sz);

static bool block_is_big_enough(size_t query, struct block_header *block) { return block->capacity.bytes >= query; }

static size_t pages_count(size_t mem) { return mem / getpagesize() + ((mem % getpagesize()) > 0); }

static size_t round_pages(size_t mem) { return getpagesize() * pages_count(mem); }

static void block_init(void *restrict addr, block_size block_sz, void *restrict next) {
    *((struct block_header *) addr) = (struct block_header) {
            .next = next,
            .capacity = capacity_from_size(block_sz),
            .is_free = true
    };
}

static size_t region_actual_size(size_t query) { return size_max(round_pages(query), REGION_MIN_SIZE); }

extern inline bool region_is_invalid(const struct region *r);


static void *map_pages(void const *addr, size_t length, int additional_flags) {
    return mmap((void *) addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | additional_flags, 0, 0);
}

#define BLOCK_MIN_CAPACITY 24


/*  аллоцировать регион памяти и инициализировать его блоком */
static struct region alloc_region(void const *addr, size_t query) {
    bool status = true;
    const size_t region_size = region_actual_size(offsetof(struct block_header, contents));
    void *reg_adr = map_pages(addr, region_size, 0);
    if (reg_adr == MAP_FAILED || reg_adr == NULL) {
        status = false;
        reg_adr = map_pages(addr, region_size, 0);
        if (reg_adr == MAP_FAILED || reg_adr == NULL) {
            return REGION_INVALID;
        }
    }
    const struct region new_region = {.addr = reg_adr, .size = region_size, .extends = status};
    block_init(reg_adr, (block_size) {.bytes = region_size}, NULL);
    return new_region;

}

static void *block_after(struct block_header const *block);

void *heap_init(size_t initial) {
    const struct region region = alloc_region(HEAP_START, initial);
    if (region_is_invalid(&region)) return NULL;

    return region.addr;
}


/*  --- Разделение блоков (если найденный свободный блок слишком большой )--- */

static bool block_splittable(struct block_header *restrict block, size_t query) {
    return block->is_free &&
           query + offsetof(struct block_header, contents) + BLOCK_MIN_CAPACITY <= block->capacity.bytes;
}

static bool split_if_too_big(struct block_header *block, size_t query) {
    const size_t capacity_query = size_max(BLOCK_MIN_CAPACITY, query);
    if (block_splittable(block, capacity_query)) {
        const block_capacity occupied_block_capacity = {query + offsetof(struct block_header, contents)};
        const block_size remaining_space = {size_from_capacity(block->capacity).bytes - occupied_block_capacity.bytes};

        struct block_header *const next_block_header = block_after(block);
        block_init(next_block_header, remaining_space, block->next);
        block->next = next_block_header;
        return true;
    }
    return false;
}


/*  --- Слияние соседних свободных блоков --- */

static void *block_after(struct block_header const *block) {
    return (void *) (block->contents + block->capacity.bytes);
}

static bool blocks_continuous(
        struct block_header const *fst,
        struct block_header const *snd) {
    return (void *) snd == block_after(fst);
}

static bool mergeable(struct block_header const *restrict fst, struct block_header const *restrict snd) {
    return fst->is_free && snd->is_free && blocks_continuous(fst, snd);
}

static bool try_merge_with_next(struct block_header *block) {
    if (!block->next || !mergeable(block, block->next))
        return false;
    block->capacity.bytes += size_from_capacity(block->next->capacity).bytes;
    block->next = block->next->next;
    return true;
}


/*  --- ... ecли размера кучи хватает --- */

struct block_search_result {
    enum {
        BSR_FOUND_GOOD_BLOCK, BSR_REACHED_END_NOT_FOUND, BSR_CORRUPTED
    } type;
    struct block_header *block;
};


static struct block_search_result find_good_or_last(struct block_header *restrict block, size_t sz) {
    while (try_merge_with_next(block)) {
        //if the block is good, return it
        if (block->is_free && block_is_big_enough(sz, block))
            return (struct block_search_result) {.type = BSR_FOUND_GOOD_BLOCK, .block = block};

        //if block is not the last one, go to the next one
        //else return it
        if (block->next != NULL)
            find_good_or_last(block->next, sz);
        else
            return (struct block_search_result) {.type = BSR_REACHED_END_NOT_FOUND, .block = block};
    }
}

/*  Попробовать выделить память в куче начиная с блока `block` не пытаясь расширить кучу
 Можно переиспользовать как только кучу расширили. */
static struct block_search_result try_memalloc_existing(size_t query, struct block_header *block) {
    const struct block_search_result result = find_good_or_last(block, query);
    if (result.type == BSR_FOUND_GOOD_BLOCK)
        split_if_too_big(block, query);
    return result;
}


static struct block_header *grow_heap(struct block_header *restrict last, size_t query) {
    struct region new_region = alloc_region(block_after(last), query);
    if (region_is_invalid(&new_region))
        return NULL;
    if (new_region.addr == block_after(last))
        new_region.extends = true;
    last->next = new_region.addr;
    return last->next;

}

/*  Реализует основную логику malloc и возвращает заголовок выделенного блока */
static struct block_header *memalloc(size_t query, struct block_header *heap_start) {
    struct block_search_result result = try_memalloc_existing(query, heap_start);

    switch (result.type) {
        case BSR_CORRUPTED:
            return NULL;
        case BSR_FOUND_GOOD_BLOCK:
            result.block->is_free = false;
            return result.block;
        case BSR_REACHED_END_NOT_FOUND:
            grow_heap(result.block, query);
        default:
            return NULL;
    }

}

void *_malloc(size_t query) {
    struct block_header *const addr = memalloc(query, (struct block_header *) HEAP_START);
    if (addr) return addr->contents;
    else return NULL;
}

static struct block_header *block_get_header(void *contents) {
    return (struct block_header *) (((uint8_t *) contents) - offsetof(struct block_header, contents));
}

void _free(void *mem) {
    if (!mem) return;
    struct block_header *header = block_get_header(mem);
    header->is_free = true;
    while (try_merge_with_next(header));
}
