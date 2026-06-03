#include <sbi_utils/ud/basic_allocator.h>
#include <sbi/sbi_string.h>

#ifndef BASIC_ALLOCATOR_MANAGED_SPACE_SIZE
#define BASIC_ALLOCATOR_MANAGED_SPACE_SIZE 4096
#endif

#define MIN_BLOCK_SIZE 32
#define SIZE_ALIGNMENT_FACTOR 8

static __aligned(4096) char managed_space[BASIC_ALLOCATOR_MANAGED_SPACE_SIZE] = {0};

struct basic_allocator_block;

typedef struct __attribute__((aligned(8)))
{
	struct basic_allocator_block *next;
	struct basic_allocator_block *prev;
	size_t size;
	bool occupied;
} basic_allocator_block_header;

typedef struct basic_allocator_block
{
	basic_allocator_block_header header;
	void* data_start;
} basic_allocator_block;


struct basic_allocator
{
	basic_allocator_block *head;
};

static struct basic_allocator allocator = {0};

static inline size_t align_size(size_t size)
{
	bool has_tail = size % SIZE_ALIGNMENT_FACTOR > 0;
	return SIZE_ALIGNMENT_FACTOR * (size / SIZE_ALIGNMENT_FACTOR + (has_tail ? 1 : 0));
}

static basic_allocator_block *find_free_block(size_t size)
{
	basic_allocator_block *cur = allocator.head;
	while (cur != NULL) {
		if (!cur->header.occupied && cur->header.size >= size) {
			break;
		}
		cur = cur->header.next;
	}
	return cur;
}

static void split_if_needed(basic_allocator_block* blk, size_t size)
{
	bool needs_split = blk->header.size - size >= MIN_BLOCK_SIZE + offsetof(basic_allocator_block, data_start);
	if (!needs_split) {
		return;
	}
	basic_allocator_block *new_blk = (basic_allocator_block *)((char *) blk->data_start + size);
	new_blk->header.size = blk->header.size - size - offsetof(basic_allocator_block, data_start);
	blk->header.size = size;
	new_blk->header.next = blk->header.next;
	blk->header.next = new_blk;
	new_blk->header.prev = blk;
	new_blk->header.occupied = false;
	new_blk->data_start = &new_blk->data_start;
	if (new_blk->header.next != NULL) {
		new_blk->header.next->header.prev = new_blk;
	}
}

static bool can_merge_with_neighbors(basic_allocator_block* blk)
{
	bool can_merge_next = blk->header.next != NULL && !blk->header.next->header.occupied;
	bool can_merge_prev = blk->header.prev != NULL && !blk->header.prev->header.occupied;
	return can_merge_next || can_merge_prev;
}

static void merge_with_next(basic_allocator_block *blk)
{
	bool can_merge_next = blk->header.next != NULL && !blk->header.next->header.occupied;
	if (can_merge_next) {
		basic_allocator_block *next = blk->header.next;
		blk->header.size += next->header.size + offsetof(basic_allocator_block, data_start);
		blk->header.next = next->header.next;
		if (next->header.next != NULL) {
			next->header.next->header.prev = blk;
		}
	}
}

static basic_allocator_block *merge_with_neighbors(basic_allocator_block *blk)
{
	merge_with_next(blk);
	bool can_merge_prev = blk->header.prev != NULL && !blk->header.prev->header.occupied;
	if (can_merge_prev) {
		blk = blk->header.prev;
		merge_with_next(blk);
	}
	return blk;
}

void basic_allocator_init()
{
	if (allocator.head != NULL) {
		return;
	}
	allocator.head = (struct basic_allocator_block*) managed_space;
	allocator.head->header.next = NULL;
	allocator.head->header.prev = NULL;
	allocator.head->header.occupied = false;
	allocator.head->header.size = BASIC_ALLOCATOR_MANAGED_SPACE_SIZE - offsetof(basic_allocator_block, data_start);
	allocator.head->data_start = &allocator.head->data_start;
}

void *basic_malloc(size_t size)
{
	if (size == 0) {
		return NULL;
	}
	size = align_size(size);
	basic_allocator_block *blk = find_free_block(size);
	if (blk == NULL) {
		return NULL;
	}
	split_if_needed(blk, size);
	blk->header.occupied = true;
	return &blk->data_start;
}

void *basic_calloc(size_t num, size_t size)
{
	void *ptr = basic_malloc(num * size);
	if (ptr == NULL) {
		return NULL;
	}
	sbi_memset(ptr, 0, num * size);
	return ptr;
}

void basic_free(void *ptr)
{
	if (ptr == NULL) {
		return;
	}
	basic_allocator_block *blk = (basic_allocator_block*) ((char*)ptr - offsetof(basic_allocator_block, data_start));
	blk->header.occupied = false;
	while(can_merge_with_neighbors(blk)) {
		blk = merge_with_neighbors(blk);
	}
}

void *basic_realloc(void *ptr, size_t new_size)
{
	if (ptr == NULL) {
		return basic_malloc(new_size);
	}
	if (new_size == 0) {
		basic_free(ptr);
		return NULL;
	}
	basic_allocator_block *blk = (basic_allocator_block*) ((char*)ptr - offsetof(basic_allocator_block, data_start));
	size_t size_to_copy = new_size < blk->header.size ? new_size : blk->header.size;
	void *new_ptr = basic_malloc(new_size);
	sbi_memcpy(new_ptr, ptr, size_to_copy);
	basic_free(ptr);
	return new_ptr;
}
