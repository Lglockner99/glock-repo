/*------------------------------------------------------------------- 
Name: Liam Glockner
Course: ECE 322 
Assignment: 5  

I did not attempt the extra credit :(

                          
 * Lab 5 Starter code:                                                                            
 *        single doubly-linked free block list with LIFO policy                                   
 *        with support for coalescing adjacent free blocks  
 *
 * Terminology:
 * o We will implement an explicit free list allocator
 * o We use "next" and "previous" to refer to blocks as ordered in
 *   the free list.
 * o We use "following" and "preceding" to refer to adjacent blocks
 *   in memory.
 *-------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Macros for unscaled pointer arithmetic to keep other code cleaner.  
   Casting to a char* has the effect that pointer arithmetic happens at
   the byte granularity (i.e. POINTER_ADD(0x1, 1) would be 0x2).  (By
   default, incrementing a pointer in C has the effect of incrementing
   it by the size of the type to which it points (e.g. BlockInfo).)
   We cast the result to void* to force you to cast back to the 
   appropriate type and ensure you don't accidentally use the resulting
   pointer as a char* implicitly.  You are welcome to implement your
   own pointer arithmetic instead of using these macros.
*/
#define UNSCALED_POINTER_ADD(p,x) ((void*)((char*)(p) + (x)))
#define UNSCALED_POINTER_SUB(p,x) ((void*)((char*)(p) - (x)))


/******** FREE LIST IMPLEMENTATION ***********************************/


/* A BlockInfo contains information about a block, including the size
   and usage tags, as well as pointers to the next and previous blocks
   in the free list.  This is exactly the "explicit free list" structure
   illustrated in the lecture slides.
   
   Note that the next and prev pointers and the boundary tag are only
   needed when the block is free.  To achieve better utilization, mm_malloc
   should use the space for next and prev as part of the space it returns.

   +--------------+
   | sizeAndTags  |  <-  BlockInfo pointers in free list point here
   |  (header)    |
   +--------------+
   |     next     |  <-  Pointers returned by mm_malloc point here
   +--------------+
   |     prev     |
   +--------------+
   |  space and   |
   |   padding    |
   |     ...      |
   |     ...      |
   +--------------+
   | boundary tag |
   |  (footer)    |
   +--------------+
*/
struct BlockInfo {
  // Size of the block (in the high bits) and tags for whether the
  // block and its predecessor in memory are in use.  See the SIZE()
  // and TAG macros, below, for more details.
  size_t sizeAndTags;
  // Pointer to the next block in the free list.
  struct BlockInfo* next;
  // Pointer to the previous block in the free list.
  struct BlockInfo* prev;
};
typedef struct BlockInfo BlockInfo;


/* Pointer to the first BlockInfo in the free list, the list's head. 
   
   A pointer to the head of the free list in this implementation is
   always stored in the first word in the heap.  mem_heap_lo() returns
   a pointer to the first word in the heap, so we cast the result of
   mem_heap_lo() to a BlockInfo** (a pointer to a pointer to
   BlockInfo) and dereference this to get a pointer to the first
   BlockInfo in the free list. */
#define FREE_LIST_HEAD *((BlockInfo **)mem_heap_lo())

/* Size of a word on this architecture. */
#define WORD_SIZE sizeof(void*)

/* Minimum block size (to account for size header, next ptr, prev ptr,
   and boundary tag) */
#define MIN_BLOCK_SIZE (sizeof(BlockInfo) + WORD_SIZE)

/* Alignment of blocks returned by mm_malloc. */
#define ALIGNMENT 8

/* SIZE(blockInfo->sizeAndTags) extracts the size of a 'sizeAndTags' field.
   Also, calling SIZE(size) selects just the higher bits of 'size' to ensure
   that 'size' is properly aligned.  We align 'size' so we can use the low
   bits of the sizeAndTags field to tag a block as free/used, etc, like this:
   
      sizeAndTags:
      +-------------------------------------------+
      | 63 | 62 | 61 | 60 |  . . . .  | 2 | 1 | 0 |
      +-------------------------------------------+
        ^                                       ^
      high bit                               low bit

   Since ALIGNMENT == 8, we reserve the low 3 bits of sizeAndTags for tag
   bits, and we use bits 3-63 to store the size.

   Bit 0 (2^0 == 1): TAG_USED
   Bit 1 (2^1 == 2): TAG_PRECEDING_USED
*/
#define SIZE(x) ((x) & ~(ALIGNMENT - 1))

/* TAG_USED is the bit mask used in sizeAndTags to mark a block as used. */
#define TAG_USED 1 

/* TAG_PRECEDING_USED is the bit mask used in sizeAndTags to indicate
   that the block preceding it in memory is used. (used in turn for
   coalescing).  If the previous block is not used, we can learn the size
   of the previous block from its boundary tag */
#define TAG_PRECEDING_USED 2


/* Find a free block of the requested size in the free list.  Returns
   NULL if no free block is large enough. */
static void * searchFreeList(size_t reqSize) {   
  BlockInfo* freeBlock;

  freeBlock = FREE_LIST_HEAD;
  while (freeBlock != NULL){
    if (SIZE(freeBlock->sizeAndTags) >= reqSize) {
      return freeBlock;
    } else {
      freeBlock = freeBlock->next;
    }
  }
  return NULL;
}
           
/* Insert freeBlock at the head of the list.  (LIFO) */
static void insertFreeBlock(BlockInfo* freeBlock) {
  BlockInfo* oldHead = FREE_LIST_HEAD;
  freeBlock->next = oldHead;
  if (oldHead != NULL) {
    oldHead->prev = freeBlock;
  }
  //  freeBlock->prev = NULL;
  FREE_LIST_HEAD = freeBlock;
}      

/* Remove a free block from the free list. */
static void removeFreeBlock(BlockInfo* freeBlock) {
  BlockInfo *nextFree, *prevFree;
  
  nextFree = freeBlock->next;
  prevFree = freeBlock->prev;

  // If the next block is not null, patch its prev pointer.
  if (nextFree != NULL) {
    nextFree->prev = prevFree;
  }

  // If we're removing the head of the free list, set the head to be
  // the next block, otherwise patch the previous block's next pointer.
  if (freeBlock == FREE_LIST_HEAD) {
    FREE_LIST_HEAD = nextFree;
  } else {
    prevFree->next = nextFree;
  }
}

/* Coalesce 'oldBlock' with any preceeding or following free blocks. */
static void coalesceFreeBlock(BlockInfo* oldBlock) {
  BlockInfo *blockCursor;
  BlockInfo *newBlock;
  BlockInfo *freeBlock;
  // size of old block
  size_t oldSize = SIZE(oldBlock->sizeAndTags);
  // running sum to be size of final coalesced block
  size_t newSize = oldSize;

  // Coalesce with any preceding free block
  blockCursor = oldBlock;
  while ((blockCursor->sizeAndTags & TAG_PRECEDING_USED)==0) { 
    // While the block preceding this one in memory (not the
    // prev. block in the free list) is free:

    // Get the size of the previous block from its boundary tag.
    size_t size = SIZE(*((size_t*)UNSCALED_POINTER_SUB(blockCursor, WORD_SIZE)));
    // Use this size to find the block info for that block.
    freeBlock = (BlockInfo*)UNSCALED_POINTER_SUB(blockCursor, size);
    // Remove that block from free list.
    removeFreeBlock(freeBlock);

    // Count that block's size and update the current block pointer.
    newSize += size;
    blockCursor = freeBlock;
  }
  newBlock = blockCursor;

  // Coalesce with any following free block.
  // Start with the block following this one in memory
  blockCursor = (BlockInfo*)UNSCALED_POINTER_ADD(oldBlock, oldSize);
  while ((blockCursor->sizeAndTags & TAG_USED)==0) {
    // While the block is free:

    size_t size = SIZE(blockCursor->sizeAndTags);
    // Remove it from the free list.
    removeFreeBlock(blockCursor);
    // Count its size and step to the following block.
    newSize += size;
    blockCursor = (BlockInfo*)UNSCALED_POINTER_ADD(blockCursor, size);
  }
  
  // If the block actually grew, remove the old entry from the free
  // list and add the new entry.
  if (newSize != oldSize) {
    // Remove the original block from the free list
    removeFreeBlock(oldBlock);

    // Save the new size in the block info and in the boundary tag
    // and tag it to show the preceding block is used (otherwise, it
    // would have become part of this one!).
    newBlock->sizeAndTags = newSize | TAG_PRECEDING_USED;
    // The boundary tag of the preceding block is the word immediately
    // preceding block in memory where we left off advancing blockCursor.
    *(size_t*)UNSCALED_POINTER_SUB(blockCursor, WORD_SIZE) = newSize | TAG_PRECEDING_USED;  

    // Put the new block in the free list.
    insertFreeBlock(newBlock);
  }
  return;
}

/* Get more heap space of size at least reqSize. */
static void requestMoreSpace(size_t reqSize) {
  size_t pagesize = mem_pagesize();
  size_t numPages = (reqSize + pagesize - 1) / pagesize;
  BlockInfo *newBlock;
  size_t totalSize = numPages * pagesize;
  size_t prevLastWordMask;

  void* mem_sbrk_result = mem_sbrk(totalSize);
  if ((size_t)mem_sbrk_result == -1) {
    printf("ERROR: mem_sbrk failed in requestMoreSpace\n");
    exit(0);
  }
  newBlock = (BlockInfo*)UNSCALED_POINTER_SUB(mem_sbrk_result, WORD_SIZE);

  /* initialize header, inherit TAG_PRECEDING_USED status from the
     previously useless last word however, reset the fake TAG_USED
     bit */
  prevLastWordMask = newBlock->sizeAndTags & TAG_PRECEDING_USED;
  newBlock->sizeAndTags = totalSize | prevLastWordMask;
  // Initialize boundary tag.
  ((BlockInfo*)UNSCALED_POINTER_ADD(newBlock, totalSize - WORD_SIZE))->sizeAndTags = 
    totalSize | prevLastWordMask;

  /* initialize "new" useless last word
     the previous block is free at this moment
     but this word is useless, so its use bit is set
     This trick lets us do the "normal" check even at the end of
     the heap and avoid a special check to see if the following
     block is the end of the heap... */
  *((size_t*)UNSCALED_POINTER_ADD(newBlock, totalSize)) = TAG_USED;

  // Add the new block to the free list and immediately coalesce newly
  // allocated memory space
  insertFreeBlock(newBlock);
  coalesceFreeBlock(newBlock);
}


/* Print the heap by iterating through it as an implicit free list. */
static void examine_heap() {
  BlockInfo *block;

  /* print to stderr so output isn't buffered and not output if we crash */
  fprintf(stderr, "FREE_LIST_HEAD: %p\n", (void *)FREE_LIST_HEAD);

  for (block = (BlockInfo *)UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE); /* first block on hea\
									       p */
       SIZE(block->sizeAndTags) != 0 && (void*)block < (void*)mem_heap_hi();
       block = (BlockInfo *)UNSCALED_POINTER_ADD(block, SIZE(block->sizeAndTags))) {

    /* print out common block attributes */
    fprintf(stderr, "%p: %ld %ld %ld\t",
            (void *)block,
            SIZE(block->sizeAndTags),
            block->sizeAndTags & TAG_PRECEDING_USED,
            block->sizeAndTags & TAG_USED);

    /* and allocated/free specific data */
    if (block->sizeAndTags & TAG_USED) {
      fprintf(stderr, "ALLOCATED\n");
    } else {
      fprintf(stderr, "FREE\tnext: %p, prev: %p\n",
              (void *)block->next,
              (void *)block->prev);
    }
  }
  fprintf(stderr, "END OF HEAP\n\n");
}

/* Initialize the allocator. */
int mm_init () {
  // Head of the free list.
  BlockInfo *firstFreeBlock;

  // Initial heap size: WORD_SIZE byte heap-header (stores pointer to head
  // of free list), MIN_BLOCK_SIZE bytes of space, WORD_SIZE byte heap-footer.
  size_t initSize = WORD_SIZE+MIN_BLOCK_SIZE+WORD_SIZE;
  size_t totalSize;

  void* mem_sbrk_result = mem_sbrk(initSize);
  //  printf("mem_sbrk returned %p\n", mem_sbrk_result);
  if ((ssize_t)mem_sbrk_result == -1) {
    printf("ERROR: mem_sbrk failed in mm_init, returning %p\n", 
           mem_sbrk_result);
    exit(1);
  }

  firstFreeBlock = (BlockInfo*)UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE);

  // Total usable size is full size minus heap-header and heap-footer words
  // NOTE: These are different than the "header" and "footer" of a block!
  // The heap-header is a pointer to the first free block in the free list.
  // The heap-footer is used to keep the data structures consistent (see
  // requestMoreSpace() for more info, but you should be able to ignore it).
  totalSize = initSize - WORD_SIZE - WORD_SIZE;

  // The heap starts with one free block, which we initialize now.
  firstFreeBlock->sizeAndTags = totalSize | TAG_PRECEDING_USED;
  firstFreeBlock->next = NULL;
  firstFreeBlock->prev = NULL;
  // boundary tag
  *((size_t*)UNSCALED_POINTER_ADD(firstFreeBlock, totalSize - WORD_SIZE)) = totalSize | TAG_PRECEDING_USED;
  
  // Tag "useless" word at end of heap as used.
  // This is the is the heap-footer.
  *((size_t*)UNSCALED_POINTER_SUB(mem_heap_hi(), WORD_SIZE - 1)) = TAG_USED;

  // set the head of the free list to this new free block.
  FREE_LIST_HEAD = firstFreeBlock;
  return 0;
}


// TOP-LEVEL ALLOCATOR INTERFACE ------------------------------------


/* Allocate a block of size size and return a pointer to it. */
void* mm_malloc (size_t size) {
  	size_t reqSize;
  	BlockInfo * ptrFreeBlock = NULL;
  	BlockInfo * ptrNewBlock = NULL; //define new block up here so we dont have to define it later on 
  	size_t blockSize;
  	size_t precBlockUsed; //shortened this for easier understanding and typing
  	size_t difSize; //define the difSize here so we dont have to define it later

  // Zero-size requests get NULL.
  	if (size == 0) {
    		return NULL;
  	}

  // Add one word for the initial size header.
  // Note that we don't need to boundary tag when the block is used!
  	size += WORD_SIZE;

  	if (size <= MIN_BLOCK_SIZE) {
    // Make sure we allocate enough space for a blockInfo in case we
    // free this block (when we free this block, we'll need to use the
    // next pointer, the prev pointer, and the boundary tag).
   		reqSize = MIN_BLOCK_SIZE;
    
  	} else {
    // Round up for correct alignment
    		reqSize = ALIGNMENT * ((size + ALIGNMENT - 1) / ALIGNMENT);
    
  	}
  	
//========== my code starts here ========================
  
  
  	ptrFreeBlock = searchFreeList(reqSize);  //search the free list for a free block of the requested size
  
  	if (ptrFreeBlock != NULL) { // if a block of at least reqSize is found 
    		precBlockUsed = (ptrFreeBlock->sizeAndTags & TAG_PRECEDING_USED);  // set the preceeding use tag of the block to used
  	}
  	else{ //if search returns null no block was found 
    		requestMoreSpace(reqSize); //request more space of at least te requested size
    		ptrFreeBlock = searchFreeList(reqSize);     // search the free list for the block we just requested
    		precBlockUsed = TAG_PRECEDING_USED;  //set the preceeding use tag using the bit mask provided
  	}
  
  	blockSize = (size_t)SIZE(ptrFreeBlock->sizeAndTags); // get the block size from the sizeAndTags field of ptrFreeBlock
  	removeFreeBlock(ptrFreeBlock);  // now that we are using this block, we remove it from the free list
  	difSize = (size_t)SIZE(blockSize - reqSize); // size value of blocksize minus the reqsize, makes it easier when comparing and doing pointers
  	
  	
 //========= for handling the case where the block is large enough to split ==============
  	if (difSize >= MIN_BLOCK_SIZE){ //if we have enough space to make another block within this block
    		ptrNewBlock = (BlockInfo *) UNSCALED_POINTER_ADD(ptrFreeBlock, reqSize); //create a new block pointer
    		ptrNewBlock->sizeAndTags = ((difSize) | TAG_PRECEDING_USED) & (~TAG_USED); //this is a free block with the previous block, (the allocated block) holding the reqSize
    		ptrFreeBlock->sizeAndTags = reqSize | precBlockUsed; //tag preceeding used updated for allocated block ptrFreeBlock
    
    		*((size_t *) UNSCALED_POINTER_ADD(ptrFreeBlock, (reqSize - WORD_SIZE))) = reqSize;       // footer for allocated block
    		*((size_t *) UNSCALED_POINTER_ADD(ptrFreeBlock, (blockSize - WORD_SIZE))) = difSize; // footer for new block
    
    		insertFreeBlock(ptrNewBlock); //add the new freed block to the freelist
    		
  	}

  	else{
    		*((size_t *) UNSCALED_POINTER_ADD(ptrFreeBlock, (blockSize - WORD_SIZE))) = blockSize;   // footer for now allocated block
    		BlockInfo *nextBlock = (BlockInfo *) UNSCALED_POINTER_ADD(ptrFreeBlock, blockSize); //need a pointer to the next block so we can set the previous block (allocated block) used bit
    		nextBlock->sizeAndTags = (nextBlock->sizeAndTags | TAG_PRECEDING_USED); //set the previous block used bit to 1 for the next block 
  	}
  	
  	
//========= updating tags and returning pointer =============
  	ptrFreeBlock->sizeAndTags = ptrFreeBlock->sizeAndTags | (TAG_USED) | (precBlockUsed); // finally set the used bit of our allocated block to 1, and set the previous block used bits 
  
  
  	return ((void *) UNSCALED_POINTER_ADD(ptrFreeBlock, WORD_SIZE)); //return a pointer to the data within the newly allocated block, note malloc returns a void pointer so we cast it to void here
}

/* Free the block referenced by ptr. */
void mm_free (void *ptr) {
  BlockInfo * freedBlock; //changed name for easier understanding
  BlockInfo * followingBlock;
  size_t data; //size of the payload

  // Implement mm_free.  You can change or remove the declaraions
  // above.  They are included as minor hints.

//======== My code starts here ===================

//========changing the used tags and adding to the free list ===========
	freedBlock = (BlockInfo *) UNSCALED_POINTER_SUB(ptr,WORD_SIZE); //set the passed in pointer to a block info type 
	freedBlock->sizeAndTags = freedBlock->sizeAndTags & (~TAG_USED); //set the used tag as unused since we are freeing this block
	insertFreeBlock(freedBlock); //add the now free block to the free list 
	coalesceFreeBlock(freedBlock); //coalesce the block in the free list
	
	
//========= updating the next blocks precBlockUsed bits to reflect the free status of the block ============
	data = freedBlock->sizeAndTags; //size of the data
	followingBlock = (BlockInfo *) UNSCALED_POINTER_ADD(freedBlock, data); //pointer to the next block after the freedblock
	followingBlock->sizeAndTags = followingBlock->sizeAndTags & (~TAG_PRECEDING_USED); //set followingblock's prec block used bits to 0 using bit mask and bitwise &
	//printf("completed free");
	//examine_heap(); //this is for testing
}

// Implement a heap consistency checker as needed.
int mm_check() {
  return 0;
}

// Extra credit.
void* mm_realloc(void* ptr, size_t size) {
  // ... implementation here ...
  //yeah I'll pass big guy. 
  return NULL;
}
