#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
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

size_t alignment_up(size_t req_size){
        if (req_size % ALIGNMENT == 0){
                return req_size;
        }
        else {
                return req_size += ALIGNMENT - (req_size % ALIGNMENT);
        }
}

Header *get_tail(){
	Header *cur_header = head;
	while(cur_header){
		if (!cur_header->next){
			return cur_header;
		}
	}
	return NULL; 
}

int add_block(){
	void *old_break = sbrk(BLOCKSIZE);
        if (old_break == (void *) -1) {
                return 1;
        }
	uintptr_t old_int = (uintptr) oldbreak;
        if (old_int % ALIGNMENT != 0){
               	old_int += ALIGNMENT - (old_int % ALIGNMENT);
        }


	Header *tail = get_tail();
	if(tail){
		if (tail->free){
			tail->block_size += BLOCKSIZE;
		} else {
			Header *new_header = (Header *) old_int;
		
			tail->next = (Header *) old_int;
			new_header->next = NULL;
			
			new_header->block_size = BLOCKSIZE 
				- alignment_up(sizeof(Header));
			
			new_header->free = 1;
		
	}
	else {
		head = (Header *) old_int;
                head->block_size = BLOCK_SIZE;
                head->free = 1;
                head->next = NULL;
	
	}
}


Header *scan_headers_size(size_t target_size){
	Header *cur_header = head;
	while(cur_header){
		if (cur_header->block_size 
		&& cur_header->block_size > target_size
		&& cur_header->free){
			return cur_header;
		} 
		else if (cur_header->next){
			cur_header = cur_header->next;
		}
		else {
			break;
		}
	}
	return NULL;
		
}

Header *scan_headers_address(void *init_ptr){
	Header *cur_header = head;

	while(cur_header){
		if (cur_header <= init_ptr
		&& init_ptr < cur_header + cur_header->block_size 
		+ alignment_up(sizeof(Header))){
			return cur_header;
		}
		else if (cur_header->next){
			cur_header = cur_header->next;
		}
		else{
			break;
		}
	}
	return NULL;	
}

Header *split_block(Header *init_block, size_t size){
	if (init_block && init_block->block_size
	> alignment_up(size) + alignment_up(sizeof(Header)){
		
	}
	

void *malloc(size_t size){
	if (init_head) {
		return NULL;
	}
	size_t target_size = alignment_up(size) 
		+ alignment_up(sizeof(Header)) * HEADER_PADDING;

	Header *cur_header = scan_headers_size(target_size);
	while (!cur_header){
		if(add_block()){
			errno = ENOMEM;
			return NULL;
		}
		cur_header = scan_headers_size(target_size);
	}

	Header *new_header = cur_header + target_size;

	new_header->next = cur_header->next;
	cur_header->next = new_header;

	new_header->block_size = cur_header->block_size - target_size;
	cur_header->block_size = alignment_up(size);
		
	new_header->free = 0;
	cur_header->free = 0;
	
	free(new_header);
	
	void *finalptr = cur_header + alignment_up(sizeof(Header));
		
	if(getenv("DEBUG_MALLOC")){
                pp(stderr, "MALLOC: malloc(%d)    => (ptr=%p, size=%d)\n", 
			size, finalptr, cur_header->block_size);
        }

	return finalptr;		
}

void *calloc(size_t count, size_t size){
	void *finalptr = malloc(count*size);
	if (finalptr){ 
		memset(finalptr, 0, size*count);

		if (getenv("DEBGUG_MALLOC")){
			Header *final_header = scan_headers_address(finalptr);
			pp(stderr, "MALLOC: calloc(%d,%d)   
				=> (ptr=%p, size=%d)", count, size, finalptr, 
				final_header->block_size 
				+ alignment_up(sizeof(header)));
		}
		return finalptr;
	}
	return NULL;	
}

void *realloc(void *ptr, size_t size){
	if (!ptr && size) {
		return malloc(size);
	} 
	else if (ptr && size == 0) {
		free(ptr);
		return NULL;
	}
	
	Header *original_header = scan_headers_address(ptr);
	if(original_header){
		if (original_header->block_size > size 
		+ alignment_up(sizeof(Header))){
			
		} 
			
		if(original_header->next && original_header->next->free){
			size_t merged_size = original_header->block_size
				+ original_header->next->block_size;
			
			if(merged_size >= alignment_up(size)){
				
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

