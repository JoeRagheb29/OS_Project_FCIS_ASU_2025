/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//


//*****************
// Helper Func.:
//*********************

// calc power: base^ex
int clc_pow(int base, int ex)
{
    int res = 1;
    for (int i = 0; i < ex; i++)
    {
        res *= base;
    }
    return res;
}

// Find nearest power of 2 for the given size
int get_nearst_power_of_2(uint32 size)
{
    if (size == 0)
    {
        return 0;
    }
    for (int i = LOG2_MIN_SIZE; i <= LOG2_MAX_SIZE; i++)
    {
        if (clc_pow(2, i) >= size)
        {
            return clc_pow(2, i);
        }
    }
    return 0;
}

// Get the index in freeBlockLists
int get_block_list_idx(uint32 size)
{
    if (size > 0)
    {
        for (int i = LOG2_MIN_SIZE; i <= LOG2_MAX_SIZE; i++)
        {
            if (clc_pow(2, i) >= size)
            {
                return (i - LOG2_MIN_SIZE); // Start from index 0
            }
        }
    }
    return 0;
}

// get page index
int get_page_index(uint32 va)
{
    return (va - dynAllocStart) / PAGE_SIZE;
}

void *case_1(int idx_block_list){

	struct BlockElement *block = LIST_FIRST(&freeBlockLists[idx_block_list]);
    uint32 block_va = (uint32)block;

    // Remove this block from free block list
    LIST_REMOVE(&freeBlockLists[idx_block_list], block);

    // Update el free blocks num
    int pageIndex = get_page_index(block_va);
    pageBlockInfoArr[pageIndex].num_of_free_blocks -= 1;

    return (void *)block_va;
}


void init_page(int i){

    // reset page info (free is 0 & size is 0)
    pageBlockInfoArr[i].block_size = 0;
    pageBlockInfoArr[i].num_of_free_blocks = 0;

    // add page back to free pages list
    LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);
}


//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================

int numPages;
int numBlockSizes;
bool is_initialized = 0;

void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	//Comment the following line
//	panic("initialize_dynamic_allocator() Not implemented yet");

    //Set DA Limits
    dynAllocStart = daStart;
    dynAllocEnd = daEnd;

    // initialize FreePagesList
    LIST_INIT(&freePagesList);

    // initialize pages
    	// and add to free pages list
    	// set block_size and num_of_free_blocks 0
    	// add

    // calc num of pages for loop over all pages
    numPages = (daEnd - daStart) / PAGE_SIZE;

    for (int i = 0; i < numPages; i++)
    {
    	init_page(i);
    }

    // initialize all FreeBlockLists
    // BLOCK_SIZES {8, 16, 32, 64, 128, 256, 512, 1024, 2048}

    numBlockSizes = LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1; // 9
    for (int i = 0; i < numBlockSizes; i++)
    {
        LIST_INIT(&freeBlockLists[i]);
    }
}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	//Comment the following line
//	panic("get_block_size() Not implemented yet");

    // get page index
    int pageIndex = get_page_index((uint32)va);
    uint16 block_size = pageBlockInfoArr[pageIndex].block_size;

    return block_size;
}

//===========================
// [3] ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Comment the following line
//	panic("alloc_block() Not implemented yet");

	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block

    // return NULL if size is 0
    if (size == 0)
    {
        return NULL;
    }

    // get nearest power of 2
    uint32 new_size = get_nearst_power_of_2(size);
    int idx_block_list = get_block_list_idx(new_size);

    // CASE 1: if a free block exists
    if (LIST_SIZE(&freeBlockLists[idx_block_list]) > 0)
    {
        // Get first free block
    	return case_1(idx_block_list);
    }

    // CASE 2: else, if a free page exists
    else if (LIST_SIZE(&freePagesList) > 0)
    {
        // Get a free page
        struct PageInfoElement *page = LIST_FIRST(&freePagesList);
        LIST_REMOVE(&freePagesList, page);

        // Get page virtual address
        uint32 page_va = to_page_va(page);

        // Allocate page
        int ret = get_page((void *)page_va);
        if (ret != 0)
        {
            panic("alloc_block: Failed to allocate page");
        }

        // calc num of blocks in this page
        int block_num = PAGE_SIZE / new_size;

        // Initialize page info
        page->num_of_free_blocks = block_num;
        page->block_size = new_size;

        // Split page to blocks
        // add them to free list
        for (int i = 0; i < block_num; i++)
        {
            uint32 block_va = page_va + (i * new_size);
            struct BlockElement *block = (struct BlockElement *)block_va;
            LIST_INSERT_TAIL(&freeBlockLists[idx_block_list], block);
        }
        // nst5dm awl case (case_1) 3ady
        return case_1(idx_block_list);
    }

    // CASE 3: else, allocate block from the next list(s)
    else
    {
        for (int i = idx_block_list + 1; i < numBlockSizes; i++)
        {
            if (LIST_SIZE(&freeBlockLists[i]) > 0)
            {
            	// use case 1 to get block
            	return case_1(i);
            }
        }

        // CASE 4: No memory available
        panic("alloc_block: Out of memory!");
        return NULL;
    }

}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	//Comment the following line
//	panic("free_block() Not implemented yet");

    // get el page w el block size
    int page_index = get_page_index((uint32)va);
    int block_size = get_block_size(va);

    if (block_size == 0)
    {
        panic("free_block: block_size is 0 ");
    }

    // return block to corresponding free list
    int list_index = get_block_list_idx(block_size);
    struct BlockElement *block = (struct BlockElement *)va;
    LIST_INSERT_TAIL(&freeBlockLists[list_index], block);

    // increment num of free blocks
    pageBlockInfoArr[page_index].num_of_free_blocks += 1;

    int free_blocks = pageBlockInfoArr[page_index].num_of_free_blocks;

    // if page become free
    // remove all blocks from free block list
    if (free_blocks*block_size == PAGE_SIZE)
    {
        uint32 page_va = to_page_va(&pageBlockInfoArr[page_index]);

        for (int i = 0; i < (PAGE_SIZE / block_size); i++)
        {
            uint32 blk_va = page_va + (i * block_size);
            struct BlockElement *blk = (struct BlockElement *)blk_va;
            LIST_REMOVE(&freeBlockLists[list_index], blk);
        }

        // return el page (free page)
        return_page((void *)page_va);

        // reset page info and add it to free page list
        init_page(page_index);

    }
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
