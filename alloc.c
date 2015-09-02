//
//  COMP1927 Assignment 1 - Vlad: the memory allocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014, August 2015
//  Copyright (c) 2012-2015 UNSW. All rights reserved.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HEADER_SIZE    sizeof(struct free_list_header)  
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
   u_int32_t magic;  // ought to contain MAGIC_FREE
   vsize_t size;     // # bytes in this block (including header)
   vlink_t next;     // memory[] index of next free block
   vlink_t prev;     // memory[] index of previous free block
} free_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of allocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]

void checkFree(free_header_t *header);
void removeBlock(free_header_t *curr);

//function to check if power of two
u_int32_t isPowerOfTwo (u_int32_t x); 
u_int32_t nextPower(u_int32_t size);
free_header_t * freeRegionSearch(free_header_t *region, u_int32_t size);
free_header_t * splitRegion(free_header_t *curr, u_int32_t s);
free_header_t * createHeader(free_header_t *curr, u_int32_t shift);
vlink_t findIndex(free_header_t *newRegion);
void freeBlock(vlink_t freeBlockIndex, free_header_t *freeBlock, free_header_t *next, free_header_t *prev);
free_header_t * findFreeLocation(vlink_t freeBlockIndex, free_header_t *firstPoint);
void vlad_merge (free_header_t *freeMem);


// Input: size - number of bytes to make available to the allocator
// Output: none              
// Precondition: Size is a power of two.
// Postcondition: `size` bytes are now available to the allocator
// 
// (If the allocator is already initialised, this function does nothing,
//  even if it was initialised with different size)

void vlad_init(u_int32_t size)
{
   // check memory isn't initialised
   if (memory != NULL) {
      printf("vlad_init: memory already initialised\n");
      return;
   }

   //printf("size allocated is %d\n", size);

   if (isPowerOfTwo(size) != 1 || size < 512) {
       if (size < 512) {
         size = 512;
      } else {
         size = nextPower(size);
      }

   }
   memory_size = size;
   memory = malloc(size);

   if (memory == NULL) {
      fprintf(stderr, "vlad_init: insufficient memory");
      abort();
   }

   free_list_ptr = 0;
   free_header_t *header;
   header = (free_header_t *)memory;
   header->magic = MAGIC_FREE;
   header->size = size;
   //printf("header size is %d\n", header->size);
   header->prev = free_list_ptr;
   header->next = free_list_ptr;

   // TODO
   // remove the above when you implement your code
}


// Input: n - number of bytes requested
// Output: p - a pointer, or NULL
// Precondition: n is < size of memory available to the allocator
// Postcondition: If a region of size n or greater cannot be found, p = NULL 
//                Else, p points to a location immediately after a header block
//                      for a newly-allocated region of some size >= 
//                      n + header size.

void *vlad_malloc(u_int32_t n) 
{

	//printf("free list ptr is %d\n", free_list_ptr);
   n += HEADER_SIZE;

   //printf("n is %d\n", n);

   free_header_t *curr = (free_header_t *)&memory[free_list_ptr];

   checkFree(curr);

   curr = freeRegionSearch(curr, n);

   //printf("address of curr is %p\n", curr);
   //printf("curr->size is %d\n", curr->size);

   if ((curr->size)/2 >= n) {
   		curr = splitRegion(curr, n);
   } 

   free_header_t *freePoint = (free_header_t *)&memory[free_list_ptr];
   if (freePoint->next == free_list_ptr && freePoint->prev == free_list_ptr && findIndex(curr) == free_list_ptr) {
      printf("there's only one block in free list\n");
      return NULL;
   }


   removeBlock(curr);
   curr->magic = MAGIC_ALLOC;

   //printf("free list pointer is %d\n", free_list_ptr);


   // LOOK FOR REGION OF HEADERSIZE+N
	//IF IT FINDS ONES CHECK THAT IF HEADER_SIZE+N COULD FIT IN HALF THE REGION
   //IF YES SPLIT INTO TWO REGIONS
   //KEEP SPLITTING UNTIL YOU GET A REGION THAT'S S>=HEADERSIZE+N but s/2 < HEADER_SIZE+n
   //IF REGION IS THE ONLY FREE REGION THEN RETURN NULL
   //


	return ((void*)((byte *)curr + HEADER_SIZE)); // temporarily
}

void removeBlock(free_header_t *curr) {

   checkFree(curr);

   //printf("index before curr is %d\n", curr->prev);
   free_header_t *currPrev = (free_header_t *)&memory[curr->prev];
   free_header_t *currNext = (free_header_t *)&memory[curr->next];
   //printf("index after curr is %d\n", curr->next);
   currPrev->next = curr->next;
   currNext->prev = curr->prev;
   //printf("previous block %d now points to %d\n", curr->prev, currPrev->next);

   //move free_list_ptr so it's always first free block in memory
   if (findIndex(curr) == free_list_ptr) {
      free_list_ptr = curr->next;
   }

   free_header_t *newFree = (free_header_t *)&memory[free_list_ptr];
   checkFree(newFree);

}


// Input: object, a pointer.
// Output: none
// Precondition: object points to a location immediately after a header block
//               within the allocator's memory.
// Postcondition: The region pointed to by object can be re-allocated by 
//                vlad_malloc

void vlad_free(void *object)
{
	//printf("location of object is %p\n", object);
   object -= HEADER_SIZE;
   //printf("location of object is %p\n", object);

   free_header_t *freeMem = (free_header_t *)object;

   if (freeMem->magic != MAGIC_ALLOC) {
   	fprintf(stderr, "Attempt to free non-allocated memory");
   }

   vlink_t aBlockF = findIndex(freeMem);
   free_header_t *firstPoint = (free_header_t *)&memory[free_list_ptr];

   //printf("memory we want to free is at %d\n", aBlockF);

   if (aBlockF > free_list_ptr && aBlockF < firstPoint->prev) {
      free_header_t *prevBlock = findFreeLocation(aBlockF, firstPoint);
      free_header_t *nextBlock = (free_header_t *)&memory[prevBlock->next];
      //printf("block before is %d, block after is %d\n", findIndex(prevBlock), findIndex(nextBlock));
      freeBlock(aBlockF, freeMem, nextBlock, prevBlock);

   } else {
      free_header_t *nextBlock = firstPoint;
      free_header_t *prevBlock = (free_header_t *)&memory[firstPoint->prev];
      //printf("block before is %d, block after is %d\n", findIndex(prevBlock), findIndex(nextBlock));
      freeBlock(aBlockF, freeMem, nextBlock, prevBlock);
   }

   if (aBlockF < free_list_ptr) {
      free_list_ptr = aBlockF;
   }

   vlad_merge(freeMem);

}

void vlad_merge (free_header_t *freeMem) {

   vsize_t size = (freeMem->size)*2;
   vlink_t freeBlockIndex = findIndex(freeMem);

   //printf("looking to merge into a total size of %d\n", size);
   
   if (freeBlockIndex%size == 0) {
      free_header_t *nextBlock = (free_header_t *)&memory[freeMem->next];
      checkFree(nextBlock);

      if (freeBlockIndex + freeMem->size == freeMem->next && freeMem->size == nextBlock->size) {
         //printf("can merge with block after it at index %d!\n", freeMem->next);
         freeMem->size = size;
         removeBlock(nextBlock);
         vlad_merge(freeMem);
      }

   } else {
      free_header_t *prevBlock = (free_header_t *)&memory[freeMem->prev];
      checkFree(prevBlock);

      if (freeBlockIndex - freeMem->size == freeMem->prev && freeMem->size == prevBlock->size) {
         //printf("can merge with block after it at index %d!\n", freeMem->prev);
         prevBlock->size = size;
         removeBlock(freeMem);
         vlad_merge(prevBlock);
      }

   }

}




void freeBlock(vlink_t freeBlockIndex, free_header_t *freeBlock, free_header_t *next, free_header_t *prev) {

   freeBlock->next = findIndex(next);
   freeBlock->prev = findIndex(prev);
   freeBlock->magic = MAGIC_FREE;

   next->prev = freeBlockIndex;
   prev->next = freeBlockIndex;

}

free_header_t * findFreeLocation(vlink_t freeBlockIndex, free_header_t *firstPoint) {

   free_header_t *curr = firstPoint;

   while (curr->next != free_list_ptr) {
      //printf("entered while loop to find appropriate before and after blocks\n");
      if ((findIndex(curr) < freeBlockIndex) && (curr->next > freeBlockIndex)) {
         //printf("found blocks!\n");
         //printf("%d is in between %d and %d\n", freeBlockIndex, findIndex(curr), curr->next);
         return curr;
      }
      curr = (free_header_t *)&memory[curr->next];
   }

   //printf("can't find location because you've fucked up your code. returning NULL\n");
   return NULL;
}


// Stop the allocator, so that it can be init'ed again:
// Precondition: allocator memory was once allocated by vlad_init()
// Postcondition: allocator is unusable until vlad_int() executed again

void vlad_end(void)
{
   free(memory);
}


// Precondition: allocator has been vlad_init()'d
// Postcondition: allocator stats displayed on stdout

void vlad_stats(void)
{
   // TODO
   // put whatever code you think will help you
   // understand Vlad's current state in this function
   // REMOVE all pfthese statements when your vlad_malloc() is done
   printf("vlad_stats() won't work until vlad_malloc() works\n");
   return;
}


free_header_t * splitRegion(free_header_t *curr, u_int32_t s) {

	//printf("splitting region of size %d\n into region of less than %d\n", curr->size, s);
	
	while ((curr->size)/2 > s) {
		u_int32_t shift = (curr->size)/2;
		//printf("allocing memory %d away from original header\n", shift);

		free_header_t *newRegion = createHeader(curr, shift);
		
		//printf("new header is at %p\n", newRegion);
		
		curr->next = findIndex(newRegion);

		//printf("new header should be at %p\n", &memory[curr->next]);

		curr->size = shift;
	  }

	return curr;
}

free_header_t * createHeader(free_header_t *curr, u_int32_t shift) {
	byte *split = (byte *)curr + shift;
	free_header_t *newHeader = (free_header_t *)(split);
	newHeader->magic = MAGIC_FREE;
	newHeader->size = shift;
	newHeader->prev = findIndex(curr);
	newHeader->next = curr->next;

	free_header_t *headAfter = (free_header_t *)&memory[newHeader->next];
	headAfter->prev = findIndex(newHeader);

	//newHeader->next->prev

	return newHeader;

}

vlink_t findIndex(free_header_t *newRegion) {
	
	byte *newR = (byte *)newRegion;
	vlink_t difference = newR - memory;	

	return difference;

}


free_header_t * freeRegionSearch(free_header_t *region, u_int32_t size) {

  free_header_t *curr = region;

  if (free_list_ptr == 0 && memory[curr->next] == memory[free_list_ptr] && memory[curr->prev] == memory[free_list_ptr]) {
  	//printf("Free list hasn't been allocated any space yet.\n");
   if (curr->size >= size) {
        return curr;
      } else {
      	fprintf(stderr, "Memory corruption\n");
		abort();
      }
  } 

  //printf("Searching for free region.\n");
   do {  	
     //printf("index of region being searched is %d\n", findIndex(curr));
	   checkFree(curr);

      if (curr->size >= size) {
   	   //printf("curr is the right size!\n");
  		   return curr;
  	   } 

  	   //printf("moving on to next block\n");
  	   curr = (free_header_t *)&memory[curr->next];

  } while (findIndex(curr) != free_list_ptr);
  
  //printf("no region of correct size found!\n");
  curr = NULL;

  return curr;
}



void checkFree(free_header_t *header) {
   if (header->magic != MAGIC_FREE) {
      //printf("testing that it is a free area\n");
      fprintf(stderr, "Memory corruption: block isn't free\n");
      abort();
   }
}


//
// All of the code below here was written by Alen Bou-Haidar, COMP1927 14s2
//

//
// Fancy allocator stats
// 2D diagram for your allocator.c ... implementation
// 
// Copyright (C) 2014 Alen Bou-Haidar <alencool@gmail.com>
// 
// FancyStat is free software: you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or 
// (at your option) any later version.
// 
// FancyStat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>


#include <string.h>

#define STAT_WIDTH  32
#define STAT_HEIGHT 16
#define BG_FREE      "\x1b[48;5;35m" 
#define BG_ALLOC     "\x1b[48;5;39m"
#define FG_FREE      "\x1b[38;5;35m" 
#define FG_ALLOC     "\x1b[38;5;39m"
#define CL_RESET     "\x1b[0m"


typedef struct point {int x, y;} point;

static point offset_to_point(int offset,  int size, int is_end);
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label);



// Print fancy 2D view of memory
// Note, This is limited to memory_sizes of under 16MB
void vlad_reveal(void *alpha[26])
{
    int i, j;
    vlink_t offset;
    char graph[STAT_HEIGHT][STAT_WIDTH][20];
    char free_sizes[26][32];
    char alloc_sizes[26][32];
    char label[3]; // letters for used memory, numbers for free memory
    int free_count, alloc_count, max_count;
    free_header_t * block;

	// TODO
	// REMOVE these statements when your vlad_malloc() is done
    //printf("vlad_reveal() won't work until vlad_malloc() works\n");
    //return;

    // initilise size lists
    for (i=0; i<26; i++) {
        free_sizes[i][0]= '\0';
        alloc_sizes[i][0]= '\0';
    }

    // Fill graph with free memory
    offset = 0;
    i = 1;
    free_count = 0;
    while (offset < memory_size){
        block = (free_header_t *)(memory + offset);
        if (block->magic == MAGIC_FREE) {
            snprintf(free_sizes[free_count++], 32, 
                "%d) %d bytes", i, block->size);
            snprintf(label, 3, "%d", i++);
            fill_block(graph, offset,label);
        }
        offset += block->size;
    }

    // Fill graph with allocated memory
    alloc_count = 0;
    for (i=0; i<26; i++) {
        if (alpha[i] != NULL) {
            offset = ((byte *) alpha[i] - (byte *) memory) - HEADER_SIZE;
            block = (free_header_t *)(memory + offset);
            snprintf(alloc_sizes[alloc_count++], 32, 
                "%c) %d bytes", 'a' + i, block->size);
            snprintf(label, 3, "%c", 'a' + i);
            fill_block(graph, offset,label);
        }
    }

    // Print all the memory!
    for (i=0; i<STAT_HEIGHT; i++) {
        for (j=0; j<STAT_WIDTH; j++) {
            printf("%s", graph[i][j]);
        }
        printf("\n");
    }

    //Print table of sizes
    max_count = (free_count > alloc_count)? free_count: alloc_count;
    printf(FG_FREE"%-32s"CL_RESET, "Free");
    if (alloc_count > 0){
        printf(FG_ALLOC"%s\n"CL_RESET, "Allocated");
    } else {
        printf("\n");
    }
    for (i=0; i<max_count;i++) {
        printf("%-32s%s\n", free_sizes[i], alloc_sizes[i]);
    }

}

// Fill block area
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label)
{
    point start, end;
    free_header_t * block;
    char * color;
    char text[3];
    block = (free_header_t *)(memory + offset);
    start = offset_to_point(offset, memory_size, 0);
    end = offset_to_point(offset + block->size, memory_size, 1);
    color = (block->magic == MAGIC_FREE) ? BG_FREE: BG_ALLOC;

    int x, y;
    for (y=start.y; y < end.y; y++) {
        for (x=start.x; x < end.x; x++) {
            if (x == start.x && y == start.y) {
                // draw top left corner
                snprintf(text, 3, "|%s", label);
            } else if (x == start.x && y == end.y - 1) {
                // draw bottom left corner
                snprintf(text, 3, "|_");
            } else if (y == end.y - 1) {
                // draw bottom border
                snprintf(text, 3, "__");
            } else if (x == start.x) {
                // draw left border
                snprintf(text, 3, "| ");
            } else {
                snprintf(text, 3, "  ");
            }
            sprintf(graph[y][x], "%s%s"CL_RESET, color, text);            
        }
    }
}

// Converts offset to coordinate
static point offset_to_point(int offset,  int size, int is_end)
{
    int pot[2] = {STAT_WIDTH, STAT_HEIGHT}; // potential XY
    int crd[2] = {0};                       // coordinates
    int sign = 1;                           // Adding/Subtracting
    int inY = 0;                            // which axis context
    int curr = size >> 1;                   // first bit to check
    point c;                                // final coordinate
    if (is_end) {
        offset = size - offset;
        crd[0] = STAT_WIDTH;
        crd[1] = STAT_HEIGHT;
        sign = -1;
    }
    while (curr) {
        pot[inY] >>= 1;
        if (curr & offset) {
            crd[inY] += pot[inY]*sign; 
        }
        inY = !inY; // flip which axis to look at
        curr >>= 1; // shift to the right to advance
    }
    c.x = crd[0];
    c.y = crd[1];
    return c;
}

u_int32_t isPowerOfTwo (u_int32_t x) { //is function a power of two
   unsigned int powerOfTwo = 1;

   while (powerOfTwo < x && powerOfTwo < 2147483648) {
      powerOfTwo *= 2;
   }
   
   return (x == powerOfTwo);
}

u_int32_t nextPower(u_int32_t size) { //finds next power of two

   size -= 1;
   size |= (size >> 1);
   size |= (size >> 2);
   size |= (size >> 4);
   size |= (size >> 8);
   size |= (size >> 16);

   size += 1;
   
   return size;

}

