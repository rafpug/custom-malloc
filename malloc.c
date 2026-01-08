#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pp.h>

#define BLOCK_SIZE 64000
#define ALIGNMENT 16
#define HEADER_PADDING 1 // The number of headers worth of space a block needs
			// to be considered splitted during malloc
typedef struct Header {
	size_t block_size;
	int free;
	struct Header *next;
} Header;

Header *head = NULL;

int init_head(){
	if (!head){
		void *old_break = sbrk(BLOCKSIZE);
		if (old_break == (void *) -1) {
			return 1;
		}
		uintptr_t old_int = (uintptr) oldbreak;
		if (old_int % ALIGNMENT != 0){
			old_int += ALIGNMENT - (old_int % ALIGNMENT);
		}
		head = (Header *) old_int;
		head->block_size = BLOCK_SIZE;
		head->free = 1;
		head->next = NULL;
	}
	return 0;
}

size_t alignment_up(size_t req_size){
	if (req_size % ALIGNMENT == 0){
		return req_size;
	} 
	else {
		return req_size += ALIGNMENT - (req_size % ALIGNMENT);
	}
}

Header *scan_headers_size(size_t target_size){
	Header *cur_header = head
	
	while(cur_header){
		if (cur_header->block_size 
		&& cur_header->block_size > target_size){
			return cur_header;
		} 
		else if (cur_header->next){
			cur_header = next;
		}
		else {
			return NULL;
		}
	}
		
}

void *malloc(size_t size){
	if (init_head) {
		return NULL;
	}
	size_t target_size = alignment_up(size) 
		+ alignment_up(sizeof(Header)) * HEADER_PADDING;

	Header *cur_header = NULL;
	if ((cur_header = scan_headers_size(target_size))){
		Header *new_header = cur_header + target_size;

		new_header->next = cur_header->next;
		cur_header->next = new_header;

		new_header->block_size = cur_header->block_size - target_size;
		cur_header->block_size = alignment_up(size);
		
		new_header->free = 1;
		cur_header->free = 0;
		
		return cur_header;
	}		
}

void *calloc(size_t count, size_t size){
	return malloc(count*size);	
}

void *realloc(void *ptr, size_t size){
	if (!ptr && size) {
		return malloc(size);
	} 
	else if (ptr && size == 0) {
		free(ptr)
		return NULL
	}
}

void free(void *ptr){
	if (!ptr){
		return;
	}
}


int test() {
	if(getenv("DEBUG_MALLOC")){
		pp(stderr, "Skeleton\n");
	}
	return 0;
}

