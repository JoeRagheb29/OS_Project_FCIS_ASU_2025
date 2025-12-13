#include "kheap.h"
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#include <inc/queue.h>
#include <inc/environment_definitions.h>
#include <kern/proc/user_environment.h>

#define my_kern_max 0XF9401000
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)

struct Segment {
	uint32 size_in_number_of_pages;
	uint32 base_address;
	uint32 index;
	uint32 is_used;
	LIST_ENTRY(Segment)
	prev_next_info;
};

//for allocated
LIST_HEAD(SegmentInfoElement_List, Segment);
struct SegmentInfoElement_List segments_list;

//for free
LIST_HEAD(SegmentfreeElement_List, Segment);
struct SegmentfreeElement_List free_segments_list;

struct empty_linked_list elist;
struct empty_linked_list alist;

LIST_HEAD(empty_linked_list, Segment);

struct Segment Segments_array[1000000];

uint32 counter = 0;

struct kspinlock lk;

// hash map store each Frame as PA to it crossponding VA
//el array de mtnf3sh feh araay built in esmha frames_info[] ast5dmha 3ltool bdl de
struct FrameInfo* reverse_hashmap[2^32] ;


void kheap_init()
{

	init_kspinlock(&lk, "kname");
	LIST_INIT(&elist);
	LIST_INIT(&alist);

	//==================================================================================
	// DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}
	//==================================================================================
	//==================================================================================
}


//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	// RAGHEB CODE //
	//handling the physical address frame
//	uint32 pa = kheap_physical_address((uint32) va);
//	uint32 pa_frame = pa >> 12;

	//assign the hash map as pa to va for O(1)
//	frames_info[pa_frame].base_virual_address = ROUNDDOWN((uint32)va, PAGE_SIZE) ;
//	reverse_hashmap[pa_frame]->base_virual_address = ROUNDDOWN((uint32)va, PAGE_SIZE) ;
//
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}


uint32 get_page_address_after_adding(uint32 va, int num_of_blocks){
	for(int i = 0; i < num_of_blocks; i++){
		va = va + PAGE_SIZE;
	}
	return va;
}


//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
	// RAGHEB //
//	uint32 pa = kheap_physical_address((uint32) va);
//	uint32 pa_frame = pa >> 12;

	// make the va of any unmapped frame in the hashmap = null
//	frames_info[pa_frame].base_virual_address = (uint32) NULL ;
//	reverse_hashmap[pa_frame]->base_virual_address = (uint32) NULL ;
}


// helper: calculate number of pages required for `size`
static inline uint32 calculate_number_of_pages(uint32 size)
{
	return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

//void my_get_page(uint32 va, uint32 num_of_pages){
//	for(uint32 i = 0; i < num_of_pages; i++){
//		get_page((uint32*)va);
//		va = va + PAGE_SIZE;
//	}
//}

uint32 max_free_index = 0;
//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
void *kmalloc(unsigned int size)
{
	acquire_kspinlock(&lk);

	if (size == 0)
	{
		release_kspinlock(&lk);
		return NULL;
	}

	// Use dynamic allocator for small sizes
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		uint32 *al = alloc_block(size);
		release_kspinlock(&lk);
		return al;
	}

	uint32 num_pages = calculate_number_of_pages(size);

	// Quick OOM check
	if (kheapPageAllocBreak == KERNEL_HEAP_MAX)
	{
		release_kspinlock(&lk);
		return NULL;
	}

    /* 1) Try exact fit in free list (elist) */
    struct Segment *free_seg = LIST_FIRST(&elist);
    for (; free_seg != 0; free_seg = LIST_NEXT(free_seg)) {
        if (free_seg->size_in_number_of_pages == num_pages) {
            uint32 alloc_base = free_seg->base_address;

            /* allocate pages */
            for (uint32 i = 0; i < num_pages; ++i)
                get_page((void*)(alloc_base + i * PAGE_SIZE));

            /* remove free segment from free list */
            LIST_REMOVE(&elist, free_seg);

            /* create allocated segment record from pool */
            struct Segment *new_alloc = &Segments_array[counter];
            new_alloc->base_address = alloc_base;
            new_alloc->size_in_number_of_pages = num_pages;
            new_alloc->is_used = 1;
            new_alloc->index = counter;

            LIST_INSERT_TAIL(&alist, new_alloc);
            counter++;

            release_kspinlock(&lk);
            return (void*)alloc_base;
        }
    }

    /* 2) Worst-fit: find largest free segment that can fit */
    uint32 biggest = 0;
    struct Segment *worst = NULL;
    free_seg = LIST_FIRST(&elist);
    for (; free_seg != 0; free_seg = LIST_NEXT(free_seg)) {
        if (free_seg->size_in_number_of_pages >= num_pages
            && free_seg->size_in_number_of_pages > biggest) {
            biggest = free_seg->size_in_number_of_pages;
            worst = free_seg;
        }
    }

    if (worst != NULL) {
        /* allocate from the beginning of worst (keep deterministic) */
        uint32 alloc_base = worst->base_address;

        /* allocate pages */
        for (uint32 i = 0; i < num_pages; ++i)
            get_page((void*)(alloc_base + i * PAGE_SIZE));

        /* adjust the free segment in-place:
           if exact match, remove it; otherwise move its base forward and shrink size */
        if (worst->size_in_number_of_pages == num_pages) {
            LIST_REMOVE(&elist, worst);
        } else {
            worst->base_address += num_pages * PAGE_SIZE;
            worst->size_in_number_of_pages -= num_pages;
        }

        /* create allocated segment record */
        struct Segment *new_alloc = &Segments_array[counter];
        new_alloc->base_address = alloc_base;
        new_alloc->size_in_number_of_pages = num_pages;
        new_alloc->is_used = 1;
        new_alloc->index = counter;

        LIST_INSERT_TAIL(&alist, new_alloc);
        counter++;

        release_kspinlock(&lk);
        return (void*)alloc_base;
    }

    /* 3) If no suitable free hole, expand the heap (kheapPageAllocBreak) */
    /* check space */
    uint32 holdkbead = kheapPageAllocBreak;

        //check insufficient space
        for(uint32 i = 0; i < num_pages; i++){
        	if(holdkbead == KERNEL_HEAP_MAX){
        		release_kspinlock(&lk);
        		return NULL;
        	}
        	holdkbead += PAGE_SIZE;
        }

    uint32 alloc_base = kheapPageAllocBreak;
    for (uint32 i = 0; i < num_pages; ++i) {
        get_page((void*)(kheapPageAllocBreak));
        kheapPageAllocBreak += PAGE_SIZE;
    }

    struct Segment *new_alloc = &Segments_array[counter];
    new_alloc->base_address = alloc_base;
    new_alloc->size_in_number_of_pages = num_pages;
    new_alloc->is_used = 1;
    new_alloc->index = counter;

    LIST_INSERT_TAIL(&alist, new_alloc);
    counter++;

    release_kspinlock(&lk);
    return (void*)alloc_base;
}


static void shrink_heap_if_possible(void)
{
    /* keep looping because removing one hole may reveal another that now touches the break */
    struct Segment *f;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (f = LIST_FIRST(&elist); f != NULL; f = LIST_NEXT(f)) {
            uint32 f_end = f->base_address + f->size_in_number_of_pages * PAGE_SIZE;
            if (f_end == kheapPageAllocBreak) {
                /* shrink the break */
                kheapPageAllocBreak -= f->size_in_number_of_pages * PAGE_SIZE;

                /* remove this free hole from the free list and clear the entry */
                LIST_REMOVE(&elist, f);
                f->is_used = 0;
                f->base_address = 0;
                f->size_in_number_of_pages = 0;

                /* we changed something; restart scan */
                changed = 1;
                break;
            }
        }
    }
}




//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
    //TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
    acquire_kspinlock(&lk);

    uint32 va = (uint32)virtual_address;

    if (virtual_address == NULL || (uint32)virtual_address == 0) {
        release_kspinlock(&lk);
        return;
    }

    /* Dynamic allocator region? */
    if (va >= KERNEL_HEAP_START && va < (KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE)) {
        free_block(virtual_address);
        release_kspinlock(&lk);
        return;
    }


    /* validity check */
    if (va < kheapPageAllocStart || va >= KERNEL_HEAP_MAX) {
        release_kspinlock(&lk);
        kpanic_into_prompt("invalid address");
        return;
    }

    /* find the allocated segment in alist by base address */
    struct Segment *seg;
    struct Segment *target = NULL;

    for (seg = LIST_FIRST(&alist); seg != NULL; seg = LIST_NEXT(seg)) {
        if (seg->base_address == va) {
            target = seg;
            break;
        }
    }

    if (target == NULL) {
        release_kspinlock(&lk);
        kpanic_into_prompt("no address found");
        return;
    }

    /* unmap pages */
    uint32 free_base = target->base_address;
    for (uint32 i = 0; i < target->size_in_number_of_pages; ++i) {
        return_page((void*)(free_base + i * PAGE_SIZE));
    }

    /* If the freed block is at the top of the heap (just below kheapPageAllocBreak),
       shrink the break instead of keeping it as a free hole */
    uint32 block_end = target->base_address + target->size_in_number_of_pages * PAGE_SIZE;
    if (block_end == kheapPageAllocBreak) {
        /* remove target from allocated list */
        LIST_REMOVE(&alist, target);

        kheapPageAllocBreak -= target->size_in_number_of_pages * PAGE_SIZE;

        /* mark target entry free */
        target->is_used = 0;
        target->size_in_number_of_pages = 0;
        target->base_address = 0;

        /* after shrinking the break, there still may be other free holes touching the break */
        shrink_heap_if_possible();

        release_kspinlock(&lk);
        return;
    }

    /* Otherwise, prepare to insert/merge into elist (reuse target as freed node marker) */
    target->is_used = 0;

    /* find neighbors in elist by addresses */
    struct Segment *left = NULL;
    struct Segment *right = NULL;
    struct Segment *f = LIST_FIRST(&elist);

    for (; f != NULL; f = LIST_NEXT(f)) {
        uint32 f_base = f->base_address;
        uint32 f_end = f_base + f->size_in_number_of_pages * PAGE_SIZE;

        if (f_end == target->base_address) {
            left = f;
        }
        if (target->base_address + target->size_in_number_of_pages * PAGE_SIZE == f_base) {
            right = f;
        }
        if (left && right) break;
    }

    /* Merge cases */
    if (left && right) {
        /* left + target + right -> merge into left */
        left->size_in_number_of_pages = left->size_in_number_of_pages
                                      + target->size_in_number_of_pages
                                      + right->size_in_number_of_pages;

        /* remove right from free list */
        LIST_REMOVE(&elist, right);
        right->is_used = 0;
        right->base_address = 0;
        right->size_in_number_of_pages = 0;

        /* remove target from allocated list (its storage now represented by left) */
        LIST_REMOVE(&alist, target);
        target->is_used = 0;
        target->base_address = 0;
        target->size_in_number_of_pages = 0;

        /* Now check whether left touches the break and shrink if needed */
        shrink_heap_if_possible();

        release_kspinlock(&lk);
        return;
    } else if (left) {
        /* left + target -> extend left */
        left->size_in_number_of_pages += target->size_in_number_of_pages;

        LIST_REMOVE(&alist, target);
        target->is_used = 0;
        target->base_address = 0;
        target->size_in_number_of_pages = 0;

        /* left may now touch the break */
        shrink_heap_if_possible();

        release_kspinlock(&lk);
        return;
    } else if (right) {
        /* target + right -> move right base down and expand it */
        right->base_address = target->base_address;
        right->size_in_number_of_pages += target->size_in_number_of_pages;

        LIST_REMOVE(&alist, target);
        target->is_used = 0;
        target->base_address = 0;
        target->size_in_number_of_pages = 0;

        /* right (now shifted) may touch the break */
        shrink_heap_if_possible();

        release_kspinlock(&lk);
        return;
    } else {
        /* no neighbors: move target from alist to elist */
        LIST_REMOVE(&alist, target);
        LIST_INSERT_TAIL(&elist, target);

        /* new free hole might touch the break */
        shrink_heap_if_possible();

        release_kspinlock(&lk);
        return;
    }
    //	panic("kfree() is not implemented yet...!!");
}



//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
		//	panic("kheap_virtual_address() is not implemented yet...!!");

		uint32 FrameNumber = physical_address >> 12;
	    uint32 VA_base = (uint32) frames_info[FrameNumber].base_virual_address;
	    uint32 offset = PGOFF(physical_address);

	    if (VA_base == 0) {
	        return 0;
	    }

	    uint32 VA_address = VA_base + offset;
	    return VA_address;
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
//	uint32* ptr_page_directory = (uint32*) rcr3();
	uint32* ptr_Page_Table = NULL;
	uint32  phyAddress = 0;

	//(get page table) btlf 3la kol row fe gadwl el directories tshof ae el address
	//ele 2osad el PDX ll Virtual Address w t7oto fel ptrPageTable
	uint32 checkPage = get_page_table(ptr_page_directory, virtual_address, &ptr_Page_Table);

	if(checkPage != 0 || ptr_Page_Table == NULL) {
		return 0;
	}

	//hageb el goz2 el PTX mn el virtual address 34an asearch beh fel page table bta3te
	uint32 phyFrameBeforeHandling = ptr_Page_Table[PTX(virtual_address)];
	if(!(phyFrameBeforeHandling & PERM_PRESENT)) {
		return 0;
	}

	// shift right for ignoring flags and permissions shift left for return the original numbers as 0000
	uint32 physicalFrameNumber = phyFrameBeforeHandling >> 12;
	uint32 physicalFrameBase = physicalFrameNumber << 12;

	uint32 offset = PGOFF(virtual_address);
//	panic("kheap_physical_address() is not implemented yet...!!");
//
//	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
	phyAddress = (uint32) physicalFrameBase + offset ;
	return phyAddress;
}




//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
