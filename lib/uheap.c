#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

int __firstTimeFlag = 1;
//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//


struct Segment {
	uint32 size;
	uint32 base_address;
	LIST_ENTRY(Segment) prev_next_info;
};

struct Segment *new_Seg;

//for allocated
LIST_HEAD(SegmentInfoElement_List, Segment);
struct SegmentInfoElement_List segments_list;

//for free
LIST_HEAD(SegmentfreeElement_List, Segment);
struct SegmentfreeElement_List free_segments_list;


static void *custom_fit(uint32 rounded_size)
{
    // ========================================================================
    // STEP 1: Try EXACT FIT in free segments list
    // ========================================================================
    struct Segment *seg;
    LIST_FOREACH(seg, &free_segments_list)
    {
        if (seg->size == rounded_size)
        {
            uint32 va_address = seg->base_address;

            // Remove from free list (exact fit - use entire segment)
            LIST_REMOVE(&free_segments_list, seg);

            return (void *)va_address;
        }
    }

    // ========================================================================
    // STEP 2: Try WORST FIT (largest block that fits)
    // ========================================================================
    struct Segment *worst_seg = NULL;
    uint32 biggest_size = 0;

    // Find the segment with the LARGEST size that can fit our allocation
    LIST_FOREACH(seg, &free_segments_list)
    {
        if (seg->size >= rounded_size && seg->size > biggest_size)
        {
            biggest_size = seg->size;
            worst_seg = seg;
        }
    }

    // If worst fit block found
    if (worst_seg != NULL)
    {
        uint32 alloc_address = worst_seg->base_address;

        // Adjust the segment (allocate from the BEGINNING)
        worst_seg->base_address += rounded_size;
        worst_seg->size -= rounded_size;

        // If segment becomes empty after allocation, remove it
        if (worst_seg->size == 0)
        {
            LIST_REMOVE(&free_segments_list, worst_seg);
        }

        return (void *)alloc_address;
    }

    // ========================================================================
    // STEP 3: Extend the heap break
    // ========================================================================

    // Check if we have enough space to extend
    if ((uheapPageAllocBreak + rounded_size) > USER_HEAP_MAX)
    {
        return NULL; // Heap is full
    }

    // Allocate by extending the break
    uint32 alloc_address = uheapPageAllocBreak;
    uheapPageAllocBreak += rounded_size;

    return (void *)alloc_address;
}



////=================================
//// [1] ALLOCATE SPACE IN USER HEAP: ragheb
////=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) {
		return NULL ;
	}

	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	// [1] Handle Small Allocations (Block Allocator)
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		return alloc_block(size);
	}

	uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);

	if (rounded_size > (USER_HEAP_MAX - USER_HEAP_START)) {

	    return NULL;
	}


	if (LIST_SIZE(&free_segments_list) > 0)
	{
		struct Segment *seg_exact;

		LIST_FOREACH(seg_exact, &free_segments_list)
		{
			if (seg_exact->size == rounded_size)
			{
				uint32 va_address = seg_exact->base_address;

				LIST_REMOVE(&free_segments_list, seg_exact);
				sys_allocate_user_mem(va_address, rounded_size);

				seg_exact->base_address = va_address;
				seg_exact->size = rounded_size;
                LIST_INSERT_TAIL(&segments_list, seg_exact);

				return (void*)va_address;
			}
		}

        struct Segment *seg_worst = NULL;
        uint32 biggest_size = 0;
        LIST_FOREACH(seg_exact, &free_segments_list)
        {
            if (seg_exact->size >= rounded_size && seg_exact->size > biggest_size)
            {
                biggest_size = seg_exact->size;
                seg_worst = seg_exact;
            }
        }

		// If the worst fit block found
		if (seg_worst != NULL)
		{
			uint32 wf_address = seg_worst->base_address;

			            sys_allocate_user_mem(wf_address, rounded_size);

			            // new segment for allocated memory
			            struct Segment *new_seg = (struct Segment *)alloc_block(sizeof(struct Segment));
			            if (new_seg == NULL)
			            {
			                return NULL;
			            }
			            new_seg->base_address = wf_address;
			            new_seg->size = rounded_size;
			            LIST_INSERT_TAIL(&segments_list, new_seg);

			            // Adjust the free segment
			            seg_worst->base_address += rounded_size;
			            seg_worst->size -= rounded_size;

			            if (seg_worst->size == 0)
			            {
			                LIST_REMOVE(&free_segments_list, seg_worst);
			            }

			            return (void *)wf_address;
			        }
	}


	// case 4: the user heap is full
	if ((uheapPageAllocBreak + rounded_size) > USER_HEAP_MAX)
	{
		cprintf("ana gowa el case el user heap is full ");
		return NULL;
	}

	// case 3: not found enough space
	uint32 va_address = uheapPageAllocBreak;


    // Track the allocation
    struct Segment *new_seg = (struct Segment *)alloc_block(sizeof(struct Segment));
    if (new_seg == NULL) {
        return NULL;
    }

    new_seg->base_address = va_address;
    new_seg->size = rounded_size;

    LIST_INSERT_TAIL(&segments_list, new_seg);
    uheapPageAllocBreak += rounded_size;

	sys_allocate_user_mem(va_address, rounded_size);

    return (void *)va_address;

//	panic("malloc() is not implemented yet...!!");
}





//
//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================

/*
 * TODO: [PROJECT'25.MS2 - #3] [2] USER HEAP - free RAGHEB
 * Description:
 * Free the memory allocated at the given virtual_address.
 */
void free(void* virtual_address)
{
	uint32 va = (uint32)virtual_address;

    if (virtual_address == NULL || (uint32)virtual_address == 0) {
        return;
    }

	// check it location if in block range or page range
	if (va >= USER_HEAP_START && va < uheapPageAllocStart) {
		free_block(virtual_address);
		return;
	}

		// Find the allocated size associated with this address
		struct Segment *seg_to_free = NULL;
		struct Segment *curr = NULL;

		// Iterate allocated segments to find the target
		LIST_FOREACH(curr, &segments_list ) {
			if (curr->base_address == va) {
				seg_to_free = curr;
				break;
			}
		}

		if (seg_to_free == NULL) {
			return; // Address not found in allocated list
		}


		LIST_REMOVE(&segments_list, seg_to_free);

		sys_free_user_mem(va, seg_to_free->size);


		// the merging
		struct Segment *s_node = NULL;
		struct Segment *prev_node = NULL;
		int inserted = 0;

		LIST_FOREACH(s_node, &free_segments_list) {
			if (s_node->base_address > va) {
				LIST_INSERT_BEFORE(&free_segments_list, s_node, seg_to_free);
				inserted = 1;
				break;
			}
			prev_node = s_node;
		}

		if (inserted == 0)
		{
			LIST_INSERT_TAIL(&free_segments_list, seg_to_free);
		}

		struct Segment *next_node = LIST_NEXT(seg_to_free);
		if (next_node != NULL)
		{
			if ((seg_to_free->base_address + seg_to_free->size) == next_node->base_address)
			{
				seg_to_free->size += next_node->size;
				LIST_REMOVE(&free_segments_list, next_node);
			}
		}

		// Attempt to Merge with Previous Segment
		struct Segment *prev_seg = LIST_PREV(seg_to_free);
		if (prev_seg != NULL)
		{
			if ((prev_seg->base_address + prev_seg->size) == seg_to_free->base_address)
			{
				prev_seg->size += seg_to_free->size;
				LIST_REMOVE(&free_segments_list, seg_to_free);
				seg_to_free = prev_seg;
			}
		}

		// Check if we can reduce the Heap Break
		struct Segment *last_seg = LIST_LAST(&free_segments_list);
		if (last_seg != NULL && (last_seg->base_address + last_seg->size) == uheapPageAllocBreak)
		{
			uheapPageAllocBreak -= last_seg->size;
			LIST_REMOVE(&free_segments_list, last_seg);
		}

	if(va > USER_HEAP_MAX) {
		panic("invalid address");
	}
}

















//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================

// helper func.
void *check_id(int shared_obj_id, void *alloc_VA, uint32 alloc_size)
{
    if (shared_obj_id < 0)
    {
        // Error - return space to free list
        struct Segment *new_free_seg = (struct Segment *)alloc_block(sizeof(struct Segment));
        if (new_free_seg != NULL)
        {
            new_free_seg->base_address = (uint32)alloc_VA;
            new_free_seg->size = alloc_size;

            // Insert back into free list (sorted)
            struct Segment *seg;
            int inserted = 0;
            LIST_FOREACH(seg, &free_segments_list)
            {
                if (seg->base_address > (uint32)alloc_VA)
                {
                    LIST_INSERT_BEFORE(&free_segments_list, seg, new_free_seg);
                    inserted = 1;
                    break;
                }
            }
            if (!inserted)
            {
                LIST_INSERT_TAIL(&free_segments_list, new_free_seg);
            }
        }
        return (void *)NULL;
    }
    // Success - track the allocation
    struct Segment *new_seg = (struct Segment *)alloc_block(sizeof(struct Segment));
    if (new_seg == NULL)
    {
        return (void *)NULL;
    }
    new_seg->base_address = (uint32)alloc_VA;
    new_seg->size = alloc_size;
    LIST_INSERT_TAIL(&segments_list, new_seg);

    // if success return va
    return alloc_VA;
}

void *smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
    //==============================================================
    // DON'T CHANGE THIS CODE========================================
    uheap_init();
    if (size == 0)
        return NULL;
    //==============================================================

    // TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
    // Your code is here
    // Comment the following line
    //	panic("smalloc() is not implemented yet...!!");
    //============
    //	Apply CUSTOM FIT strategy to search the PAGE ALLOCATOR in user heap for suitable space to the required allocation size (on 4 KB BOUNDARY)
    //	if no suitable space found, return NULL
    //	Call sys_create_shared_object(...) to invoke the Kernel for allocation of shared variable
    //	RETURN:
    //	If successful, return its virtual address
    //	Else, return NULL
    //===================
    uint32 alloc_size = ROUNDUP(size, PAGE_SIZE);

    // cust0m fit
    void *alloc_VA = custom_fit(alloc_size);
    if (!alloc_VA)
    {
        return NULL;
    }
    //    create_shared_object(sharedVarName, size, isWritable,alloc_VA);
    int shared_obj_id = sys_create_shared_object(sharedVarName, size, isWritable, alloc_VA);
    // RETURN:
    //	a) ID of the shared object (its VA after masking out its msb) if success
    //	b) E_SHARED_MEM_EXISTS if the shared object already exists
    //	c) E_NO_SHARE if failed to create a shared object
    //	if (res<0){
    //		return NULL;
    //	}
    //	return alloc_VA;

    // Check return value
    return check_id(shared_obj_id, alloc_VA, alloc_size);

    //		if(shared_obj_id==E_SHARED_MEM_NOT_EXISTS || shared_obj_id==E_NO_SHARE){
    //			return NULL;
    //		}else{
    //			return alloc_VA;
    //		}
}

////========================================
//// [4] SHARE ON ALLOCATED SHARED VARIABLE:
////========================================
void *sget(int32 ownerEnvID, char *sharedVarName)
{
    //==============================================================
    // DON'T CHANGE THIS CODE========================================
    uheap_init();
    //==============================================================

    // TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
    // Your code is here
    // Comment the following line
    //	panic("sget() is not implemented yet...!!");

    //================
    //	Get the size of the shared variable (use sys_size_of_shared_object())
    //	If not exists, return NULL
    //	Apply CUSTOM FIT strategy to search the heap for suitable space (on 4 KB BOUNDARY)
    //	if no suitable space found, return NULL
    //	Call sys_get_shared_object(...) to invoke the Kernel for sharing this variable
    //	RETURN:
    //	If successful, return its virtual address
    //	Else, return NULL
    //=========================

    int size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
    // RETURN:
    //	a) If found, return size of shared object
    //	b) Else, return E_SHARED_MEM_NOT_EXISTS

    // check shared obj is exist
    if (size == E_SHARED_MEM_NOT_EXISTS)
    {
        return NULL;
    }
    // round up to get full page
    uint32 alloc_size = ROUNDUP(size, PAGE_SIZE);
    // get suitable space, va (start of allocation)
    void *alloc_VA = custom_fit(alloc_size);
    // check free space exitst
    if (!alloc_VA)
    {
        return NULL;
    }
    //	get_shared_object(ownerEnvID,sharedVarName,alloc_VA);
    int shared_obj_id = sys_get_shared_object(ownerEnvID, sharedVarName, alloc_VA);
    // RETURN:
    //	a) ID of the shared object (its VA after masking out its msb) if success
    //	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists
    // Check return value

    return check_id(shared_obj_id, alloc_VA, alloc_size);

    //	if(shared_obj_id==E_SHARED_MEM_NOT_EXISTS){
    //		return NULL;
    //	}else{
    //		return alloc_VA;
    //	}
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
