#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pp.h>

#define BLOCK_SIZE 64000
#define ALIGNMENT 16

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

#define HEADER_SIZE alignment_up(sizeof(Header))

Header *get_tail(){
	Header *cur_header = head;
	while(cur_header){
		if (!cur_header->next){
			return cur_header;
		}
		cur_header = cur_header->next;
	}
	return NULL; 
}

int add_block(){
	void *old_break = sbrk(BLOCK_SIZE);
        if (old_break == (void *) -1) {
                return 1;
        }
	uintptr_t old_int = (uintptr_t) old_break;
        if (old_int % ALIGNMENT != 0){
               	old_int += ALIGNMENT - (old_int % ALIGNMENT);
        }


	Header *tail = get_tail();
	if(tail){
		if (tail->free){
			tail->block_size += BLOCK_SIZE;
		} 
		else {
			Header *new_header = (Header *) old_int;
		
			tail->next = (Header *) old_int;
			new_header->next = NULL;
			
			new_header->block_size = BLOCK_SIZE 
				- HEADER_SIZE;
			
			new_header->free = 1;
		}
	}
	else {
		head = (Header *) old_int;
                head->block_size = BLOCK_SIZE - HEADER_SIZE;
                head->free = 1;
                head->next = NULL;
	
	}
	return 0;
}


Header *scan_headers_size(size_t target_size){
	Header *cur_header = head;
	while(cur_header){
		if (cur_header->block_size 
		&& cur_header->block_size >= target_size
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
		if ((void *) cur_header <= init_ptr
		&& (uintptr_t) init_ptr < (uintptr_t) cur_header + cur_header->block_size 
		+ HEADER_SIZE){
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

Header *merge_next_block(Header *header1){
	pp(stderr, "merge_next_block(%p)\n", (void *)header1);
	Header *header2 = header1->next;
	if(header1 && header2 && header2->free){
		header1->next = header2->next;
		header1->block_size += header2->block_size 
			+ HEADER_SIZE;
		pp(stderr, "successful merge!!\n");
		return header1;
	}
	return NULL;
}

Header *split_block(Header *cur_header, size_t size){
        size_t target_size = HEADER_SIZE + alignment_up(size);
        if (cur_header && cur_header->block_size >= target_size){
                uintptr_t new_ptr = (uintptr_t) cur_header + target_size;
                Header *new_header = (Header *)new_ptr;
                new_header->next = cur_header->next;
                cur_header->next = new_header;

                new_header->block_size = cur_header->block_size - target_size;
                cur_header->block_size = alignment_up(size);

                new_header->free = 1;

                merge_next_block(new_header);
                return new_header;
        }
        return NULL;
}
	

void *malloc(size_t size){
	pp(stderr, "called my malloc\n");
	if (!size){
		return NULL;
	}
	size_t target_size = alignment_up(size) 
		+ HEADER_SIZE;

	Header *cur_header = scan_headers_size(target_size);
	while (!cur_header){
		if(add_block()){
			errno = ENOMEM;
			return NULL;
		}
		cur_header = scan_headers_size(target_size);
	}
	
	cur_header->free = 0;
	if(!split_block(cur_header, size)){
		return NULL;
	}
	uintptr_t finalintptr = (uintptr_t) cur_header + HEADER_SIZE;
	void *finalptr = (void *) finalintptr;
		
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
			pp(stderr, "MALLOC: calloc(%d,%d)"   
				"=> (ptr=%p, size=%d)\n", count, size, finalptr,
				final_header->block_size);
		}
		return finalptr;
	}
	return NULL;	
}
void *realloc(void *ptr, size_t size){
	pp(stderr, "called my realloc\n");
	if (!ptr && size) {
		pp(stderr, "NOpointer\nNopointer\n");
		return malloc(size);
	} 
	else if (ptr && size == 0) {
		free(ptr);
		return NULL;
	}
	
	Header *original_header = scan_headers_address(ptr);

	void *old_ptr = ptr; // Stored for debugging 

	if(original_header){
		// Case: block containing ptr was found
		uintptr_t payload_intptr = (uintptr_t) original_header + HEADER_SIZE;

		if(!split_block(original_header,size) 
		&& original_header->block_size < alignment_up(size)){
			// Case: block needs to be expanded
			if(original_header->next){
				// Case: block is not the tail
				
				while(original_header->block_size
				< alignment_up(size)){
					if(!merge_next_block(original_header)){
						break;
					}
				}
				
				if (original_header->block_size
				< alignment_up(size)){
					// Case: not enough space to realloc
					// Malloc+memcpy instead of expanding
					void *finalptr = malloc(size);
					
					if (!finalptr){
						return NULL;
					}
					memcpy(finalptr
						, (void *) payload_intptr
						, alignment_up(size));

					ptr = finalptr;
				} else {
					// Case: Successfully expanded block
					// Splits the block if it has excess
					split_block(original_header, size);
					ptr = (void *) payload_intptr; 
				} 
			}
			else{
				// Case: block is the tail of our linkedlist
				// Adds blocks to our list for more space
				original_header->free = 1;

				/* 
 				 add_block() automatically expands the tail
 				 if its free 		 
 				*/ 				
				while(original_header->block_size 
				< alignment_up(size)){
					if(!add_block()){
						errno = ENOMEM;
						return NULL;
					}
				}
				original_header->free = 0;
				split_block(original_header, size);
				ptr = (void *) payload_intptr; 
			}	
		} 
		else {
			/*
  			Case: block successfully shrank to desired size
				or was already the desired size
			*/
			ptr = (void *) payload_intptr;
		}
	}
	else {
		/*
 		 Case: block did not exist

		 Instead malloc() will be called
 		*/ 
		ptr = malloc(size);
		if (!ptr) {
			return NULL;
		}
	}

	if (getenv("DEBUG_MALLOC")){
        	Header *final_header = scan_headers_address(ptr);
                pp(stderr, "MALLOC: realloc(%p,%d) => (ptr=%p, size=%d)\n"	
			, old_ptr, size, ptr, final_header->block_size);
        }
	return ptr;			
}

void free(void *ptr){
	if (!ptr){
		return;
	}
	Header *target_header = scan_headers_address(ptr);
	if (target_header) {
		target_header->free = 1;
		merge_next_block(target_header);

		Header *prev_header = scan_headers_address(target_header-1);
		if (prev_header && prev_header->free){
			target_header = merge_next_block(prev_header);
		}

		Header *tail = get_tail();
		if (target_header == tail){
			prev_header = scan_headers_address(target_header-1);
			if (prev_header){
				prev_header->next = NULL;
			}
			else if (target_header == head){
				head = NULL;
			}
			size_t total_size = target_header->block_size
				+ HEADER_SIZE;
			pp(stderr, "shrinking the heap\n");
			if( sbrk(-1 * total_size) == (void *) -1){
				pp(stderr, "Failed to shrink heap\n");
			}
		}
	}
	pp(stderr, "MALLOC: free(%p)\n", ptr);
	return;
}


int test() {
	if(getenv("DEBUG_MALLOC")){
		pp(stderr, "Skeleton%ld\n", HEADER_SIZE);
	}
	return 0;
}

