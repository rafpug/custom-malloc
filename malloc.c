#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define BLOCK_SIZE 64000
#define ALIGNMENT 16


typedef struct Header {
	// The block size excluding the header
	size_t block_size;
	
	/* Nonzero indicates a free block
	 * Zero indicates an allocated block
 	*/
	int free;
	
	// A pointer to the next header in the list
	struct Header *next;
} Header; 

Header *head = NULL;

/* alignment_up: Accepts a size and returns the 
 * 	size at the next 16 byte step 	
 */
size_t alignment_up(size_t req_size){
        if (req_size % ALIGNMENT == 0){
                return req_size;
        }
        else {
                return req_size += ALIGNMENT - (req_size % ALIGNMENT);
        }
}

#define HEADER_SIZE alignment_up(sizeof(Header))

/* get_tail: Navigates through the linked list 
 * 	and returns the header at the tail end of the list
 */
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

/* add_block: Increases that heap size and merges 
 * 	it into our linked list
 *
 * Returns zero on success
 * Returns non-zero on failure
 */
int add_block(){
	void *old_break = sbrk(BLOCK_SIZE);
        if (old_break == (void *) -1) {
		// Failed to get memory from the OS
                return 1;
        }

	// Ensures that the pointer for our new block is 16 byte aligned
	uintptr_t old_int = (uintptr_t) old_break;
        if (old_int % ALIGNMENT != 0){
               	old_int += ALIGNMENT - (old_int % ALIGNMENT);
        }

	
 	//Adds the memory based to our linked list based on the scenario
	Header *tail = get_tail();
	if(tail){
		if (tail->free){
			// Case: Our tail is free
			// The tail block is expanded
			tail->block_size += BLOCK_SIZE;
		} 
		else {
			// Case: Our tail is allocated
			// A new node must appended to our list
			Header *new_header = (Header *) old_int;
		
			tail->next = (Header *) old_int;
			new_header->next = NULL;
			
			new_header->block_size = BLOCK_SIZE 
				- HEADER_SIZE;
			
			new_header->free = 1;
		}
	}
	else {
		// Case: we have no tail, thus no head
		// We initialize the head
		head = (Header *) old_int;
                head->block_size = BLOCK_SIZE - HEADER_SIZE;
                head->free = 1;
                head->next = NULL;
	
	}
	return 0;
}

/* scan_headers_size: Scans through our linked list 
 * 	in search of free nodes that can fit specified size
 *
 * Returns NULL when no node can fit the size
 *
 * scan_headers_size doesn't guarantee that 
 * 	the node can be split unless the inputted
 * 	size already accounts for the header_size
 */
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

/* Scans through the our linked list for a 
 * 	block that contains the given address
 *
 * The search range of a block includes the header
 *
 * Returns the header of the found block on success
 * Returns NULL if no block was found
 *
 * Mainly used to pinpoint a pointer for free() and realloc()
 * I also used it to find the previous block in the list
 */
Header *scan_headers_address(void *init_ptr){
	Header *cur_header = head;

	while(cur_header){
		if ((void *) cur_header <= init_ptr
		&& (uintptr_t) init_ptr < (uintptr_t) cur_header 
		+ cur_header->block_size 
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

/* merge_next_block: Attempts to merge the given 
 * 	block with the next block in the list
 *
 * Returns the given header upon success
 * Returns NULL when there is no next block 
 * 	or when the next block isn't free 
 */
Header *merge_next_block(Header *header1){
	Header *header2 = header1->next;
	if(header1 && header2 && header2->free){
		header1->next = header2->next;
		header1->block_size += header2->block_size 
			+ HEADER_SIZE;
		return header1;
	}
	return NULL;
}

/* split_block: Splits the given block by the given size
 * A new block is inserted into the list containing the excess
 *
 * The function accounts for both headersize 
 * 	and alignment when splitting
 *
 * Returns a pointer to the new block containing the excess
 * Returns NULL when the given block isn't big enough to be split
 *
 */
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
	
/* malloc: takes a given size and allocates it in the heap
 *
 * Returns a pointer to the allocated space upon success
 * Returns NULL upon failure
 */
void *malloc(size_t size){
	
	if (size <= 0){
		return NULL;
	}
	
	// target_size is the required space 
	// 	required for split_block() to succeed
	size_t target_size = alignment_up(size) 
		+ HEADER_SIZE;
	
	Header *cur_header = scan_headers_size(target_size);
	while (!cur_header){
		// Repeatedly adds blocks until one reaches
		// 	the target_size
		if(add_block()){
			// If add_block() returns non-zero
			// then we failed to get more memory
			// from the OS
			errno = ENOMEM;
			return NULL;
		}
		cur_header = scan_headers_size(target_size);
	}
	
	// Malloc has found a suitable block
	// 	so it marks it as allocated
	// 	and splits it to desired size
	cur_header->free = 0;
	if(!split_block(cur_header, size)){
		return NULL;
	}
	
	// Malloc finishes by calculating the pointer
	// 	to the block's payload
	uintptr_t finalintptr = (uintptr_t) cur_header + HEADER_SIZE;
	void *finalptr = (void *) finalintptr;
		
	if(getenv("DEBUG_MALLOC")){
		char buf[256];
                snprintf(buf, sizeof(buf), 
			"MALLOC: malloc(%zu)    => (ptr=%p, size=%zu)\n", 
			size, finalptr, cur_header->block_size);
		fputs(buf, stderr);
        }

	return finalptr;		
}

/* calloc: Takes a count and a size to 
 * 	allocate a zero initialized memory
 * 	in the heap
 *
 * Returns a pointer to the memory upon success
 * Returns NULL upon failure
 */
void *calloc(size_t count, size_t size){

	// Implicitly calls malloc
	void *finalptr = malloc(count*size);

	if (finalptr){ 

		// Upon a successful malloc call
		// 	memset is used to init
		// 	memory to zero
		memset(finalptr, 0, size*count);

		if (getenv("DEBUG_MALLOC")){
			Header *final_header = scan_headers_address(finalptr);

			char buf[256];
			snprintf(buf, sizeof(buf), "MALLOC: calloc(%zu,%zu)"
				"=> (ptr=%p, size=%zu)\n", 
				count, size, finalptr,
				final_header->block_size);
			fputs(buf, stderr);
		}
		return finalptr;
	}
	return NULL;	
}

/* realloc: Attempts to resize the block associated to the given
 * 	pointer or relocates data to a bigger block that fits
 * 	the given size
 *
 * 	Returns a pointer to the resized/relocated block upon success
 * 	Returns NULL upon failure
 */
void *realloc(void *ptr, size_t size){
	
	// Special cases of realloc()
	if (!ptr) {
		return malloc(size);
	} 
	else if (ptr && size <= 0) {
		free(ptr);
		return NULL;
	}
	
	Header *original_header = scan_headers_address(ptr);

	void *old_ptr = ptr; // Stored for debugging 
	
	// Goes through various checks to determine what to do with the block
	if(original_header){
		// Case: block containing ptr was found
		uintptr_t payload_intptr = (uintptr_t) original_header 
						+ HEADER_SIZE;
		
		if(!split_block(original_header,size) 
		&& original_header->block_size < alignment_up(size)){
			// Case: splitting failed, so the
			// 	block needs to be expanded
			if(original_header->next){
				// Case: block has a next block so 
				// 	it must not be the tail
				
				while(original_header->block_size
				< alignment_up(size)){
					// Repeatedly merges the next block
					// 	until it encounters one that
					// 	is already allocated

					if(!merge_next_block(original_header)){
						break;
					}
				}
				
				if (original_header->block_size
				< alignment_up(size)){
					// Case: not enough space to resize

					// Malloc+memcpy instead of expanding
					void *finalptr = malloc(size);
					
					if (!finalptr){
						return NULL;
					}
					memcpy(finalptr
						, (void *) payload_intptr
						, original_header->block_size);

					free(original_header);
					ptr = finalptr;
				} else {
					// Case: Successfully expanded block
					
					// Split off any excess space
					split_block(original_header, size);
					ptr = (void *) payload_intptr; 
				} 
			}
			else{
				// Case: block has no next, so it must be the
				// 	tail of our linked list

				// To expand we just need to add more blocks
				original_header->free = 1;

				/* 
 				 add_block() automatically expands the tail
 				 if its free 		 
 				*/ 				
				while(original_header->block_size 
				< alignment_up(size)){
					if(add_block()){
						errno = ENOMEM;
						return NULL;
					}
				}

				original_header->free = 0;
				
				// split off the excess
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
 		 Case: block containing the pointer did not exist

		 Can't realloc so we return NULL
 		*/ 
		return NULL;
	}

	if (getenv("DEBUG_MALLOC")){
        	Header *final_header = scan_headers_address(ptr);

		char buf[256];
                snprintf(buf, sizeof(buf), 
			"MALLOC: realloc(%p,%zu) => (ptr=%p, size=%zu)\n",	
			old_ptr, size, ptr, final_header->block_size);

		fputs(buf, stderr);
        }
	return ptr;			
}

/* free: Marks the block associated with the given pointer as free
 * 	Additionally merges the block with its next and previous block
 */
void free(void *ptr){
	if (!ptr){
		return;
	}
	
	// Finds the block containing the ptr
	Header *target_header = scan_headers_address(ptr);
	if (target_header) {
		
		// Marks the block as free
		target_header->free = 1;

		// Merges the next block in the list if its free
		merge_next_block(target_header);

		// Finds the previous block and merges it if its free
		Header *prev_header = scan_headers_address(target_header-1);
		if (prev_header && prev_header->free){
			target_header = merge_next_block(prev_header);
		}

		// Attempts to shrink the heap if we are freeing the tail
		// 	end of our list
		Header *tail = get_tail();
		if (target_header == tail){
			// Case: We are freeing the tail of our list
			
			// Marks the previous block, if any, as the tail
			prev_header = scan_headers_address(target_header-1);
			if (prev_header){
				prev_header->next = NULL;
			}
			else if (target_header == head){
				// Updates the head appropriately
				head = NULL;
			}
			size_t total_size = target_header->block_size
				+ HEADER_SIZE;

			sbrk(-1 * total_size);
		}
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "MALLOC: free(%p)\n", ptr);
	fputs(buf, stderr);
	return;
}
