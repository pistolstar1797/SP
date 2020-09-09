/*
2017-12751 Donghak Lee Malloc Lab

 < Allocated Block >

31 30  ......             2  1  0
┌--┬--┬--┬--┬--┬--┬--┬--┬--┬--┬--┐
|   size of the block   |  |  |  | -> header, check allocated at 0 bit
├--+--+--+--+--+--+--+--+--+--+--┤
......                                                                                               .
├--+--+--+--+--+--+--+--+--+--+--┤
|   size of the block   |  |  |  | -> footer, check allocated at 0 bit
└--┴--┴--┴--┴--┴--┴--┴--┴--┴--┴--┘

 < Free block >

31 30 ......              2  1  0
┌--┬--┬--┬--┬--┬--┬--┬--┬--┬--┬--┐
|   size of the block   |  |  |  | -> header, check allocated at 0 bit
├--+--+--+--+--+--+--+--+--+--+--┤
|                                | -> pred_ptr, access by macro
├--+--+--+--+--+--+--+--+--+--+--┤
|                                | -> succ_ptr, access by macro
├--+--+--+--+--+--+--+--+--+--+--┤
......                                                                                              .
├--+--+--+--+--+--+--+--+--+--+--┤
|   size of the block   |  |  |  | -> footer, check allocated at 0 bit
└--┴--┴--┴--┴--┴--┴--┴--┴--┴--┴--┘
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE     4
#define DSIZE     8
#define CHUNKSIZE (1<<8)

#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define MIN(x, y) ((x) < (y) ? (x) : (y)) 

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)            (*(unsigned int *)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */  
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define LIST_NUM 24

#define SET_PTR(p, bp) (*(unsigned int *)(p) = (unsigned int)(bp))

#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)

#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)(SUCC_PTR(bp)))

void **free_seg_list; 
char *heap_list; 
static void *extend_heap(size_t size);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);
static void add_node(void *bp, size_t size);
static void remove_node(void *bp);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{      
    char *heap_start;
    
    if((heap_start = mem_sbrk(LIST_NUM * WSIZE)) == NULL)
        return -1;

    free_seg_list = (void**) heap_start;
    
    for(int i=0; i<LIST_NUM; i++)
        free_seg_list[i] = NULL;

    /* Create the initial empty heap */
    if((heap_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    PUT(heap_start, 0);                            /* Alignment padding */
    PUT(heap_start + (WSIZE * 1), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_start + (WSIZE * 2), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_start + (WSIZE * 3), PACK(0, 1));     /* Epilogue header */
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /* Adjust block size to include overhead and alignment reqs. */
    size_t asize = MAX(2 * DSIZE, ALIGN(size + DSIZE));
    size_t extendsize;  /* Amount to extend heap if no fit */
   
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Search the free list for a fit */
    void *bp = NULL;
    size_t searchsize = asize;
    for (int seg_size = 0; seg_size < LIST_NUM; seg_size++) 
    {
        if (((searchsize <= 1) && (free_seg_list[seg_size] != NULL))
            ||(seg_size == LIST_NUM - 1)) {
            bp = free_seg_list[seg_size];
            
            while ((bp != NULL) && ((asize > GET_SIZE(HDRP(bp)))))
                bp = PRED(bp);

            if (bp != NULL)
                break;
        }
        searchsize /= 2;
    }

    /* No fit found. Get more memory and place the block */
    if (bp == NULL) 
    {
        extendsize = MAX(asize, CHUNKSIZE);
        if ((bp = extend_heap(extendsize)) == NULL)
            return NULL;
    }   
    
    bp = place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    add_node(bp, size);
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *new_ptr = bp;
    size_t asize = MAX(ALIGN(size + DSIZE), 2 * DSIZE);
    
    if (size == 0)
        return NULL;

    if (GET_SIZE(HDRP(bp)) < asize) {
        if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))) || !GET_SIZE(HDRP(NEXT_BLKP(bp)))) {
            int surplus = GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(bp)) - asize;
            
            if (surplus < 0) {
                if (extend_heap(MAX(-surplus, CHUNKSIZE)) == NULL)
                    return NULL;
                surplus += MAX(-surplus, CHUNKSIZE);
            }
            
            remove_node(NEXT_BLKP(bp));
            PUT(HDRP(bp), PACK(asize + surplus, 1)); 
            PUT(FTRP(bp), PACK(asize + surplus, 1)); 
        } 
        else {
            new_ptr = mm_malloc(asize - DSIZE);
            memcpy(new_ptr, bp, MIN(size, asize));
            mm_free(bp);
        }
    }

    return new_ptr;
}

static void *extend_heap(size_t size)
{
    void *bp;                   
    size_t asize = ALIGN(size);
        
    /* Allocate an even number of words to maintain alignment */
    if ((bp = mem_sbrk(asize)) == (void *)-1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(asize, 0));          /* Free block header */
    PUT(FTRP(bp), PACK(asize, 0));          /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */
    add_node(bp, asize);

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {                         /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {                   /* Case 2 */
        remove_node(bp);
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    else if (!prev_alloc && next_alloc) {                   /* Case 3 */
        remove_node(bp);
        remove_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    else {                                                  /* Case 4 */
        remove_node(bp);
        remove_node(PREV_BLKP(bp));
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    add_node(bp, size);
    
    return bp;
}


static void *place(void *bp, size_t asize)
{
    size_t ptr_size = GET_SIZE(HDRP(bp));
    size_t surplus = ptr_size - asize;
    
    remove_node(bp);

    if (surplus <= DSIZE * 2) {
        PUT(HDRP(bp), PACK(ptr_size, 1)); 
        PUT(FTRP(bp), PACK(ptr_size, 1)); 
    }
    else {
        PUT(HDRP(bp), PACK(asize,1));
        PUT(FTRP(bp), PACK(asize,1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(surplus, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(surplus, 0));
        add_node(NEXT_BLKP(bp),surplus);
        return bp;
    }
    
    return bp;
}

static void add_node(void *bp, size_t size) {

    int seg_size = 0;

    while ((seg_size < LIST_NUM - 1) && (size > 1)) {
        size /= 2;
        seg_size++;
    }
    
    void *head = free_seg_list[seg_size];
    void *prev = NULL;
    while ((head != NULL) && (size > GET_SIZE(HDRP(head)))) {
        prev = head;
        head = PRED(head);
    }

    if (head != NULL) {
        if (prev != NULL) {
            SET_PTR(PRED_PTR(bp), head);
            SET_PTR(SUCC_PTR(head), bp);
            SET_PTR(SUCC_PTR(bp), prev);
            SET_PTR(PRED_PTR(prev), bp);
        }
        else {
            SET_PTR(PRED_PTR(bp), head);
            SET_PTR(SUCC_PTR(head), bp);
            SET_PTR(SUCC_PTR(bp), NULL);
            free_seg_list[seg_size] = bp;
        }
    } 
    else {
        if (prev != NULL) {
            SET_PTR(PRED_PTR(bp), NULL);
            SET_PTR(SUCC_PTR(bp), prev);
            SET_PTR(PRED_PTR(prev), bp);
        }
        else {
            SET_PTR(PRED_PTR(bp), NULL);
            SET_PTR(SUCC_PTR(bp), NULL);
            free_seg_list[seg_size] = bp;
        }
    }
}

static void remove_node(void *bp) {
    
    size_t size = GET_SIZE(HDRP(bp));
    int seg_size = 0;

    while ((seg_size < LIST_NUM - 1) && (size > 1)) {
        seg_size++;
        size /= 2;     
    }

    if(SUCC(bp) != NULL) {
        if(PRED(bp) != NULL) {
            SET_PTR(SUCC_PTR(PRED(bp)), SUCC(bp));
            SET_PTR(PRED_PTR(SUCC(bp)), PRED(bp));
        }
        else {
            SET_PTR(PRED_PTR(SUCC(bp)), NULL);
        }
    }
    else {
        if(PRED(bp) != NULL) {
            SET_PTR(SUCC_PTR(PRED(bp)), NULL);
            free_seg_list[seg_size] = PRED(bp);
        }
        else {
            free_seg_list[seg_size] = NULL;
        }
    }
}
