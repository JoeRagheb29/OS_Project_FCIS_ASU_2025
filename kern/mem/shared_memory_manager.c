#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
// Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
    LIST_INIT(&AllShares.shares_list);
    init_kspinlock(&AllShares.shareslock, "shares lock");
    // init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
    panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
// Search for the given shared object in the "shares_list"
// Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share *find_share(int32 ownerID, char *name)
{
#if USE_KHEAP
    struct Share *ret = NULL;
    bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
    if (!wasHeld)
    {
        acquire_kspinlock(&(AllShares.shareslock));
    }
    {
        struct Share *shr;
        LIST_FOREACH(shr, &(AllShares.shares_list))
        {
            // cprintf("shared var name = %s compared with %s\n", name, shr->name);
            if (shr->ownerID == ownerID && strcmp(name, shr->name) == 0)
            {
                // cprintf("%s found\n", name);
                ret = shr;
                break;
            }
        }
    }
    if (!wasHeld)
    {
        release_kspinlock(&(AllShares.shareslock));
    }
    return ret;
#else
    panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char *shareName)
{
    // This function should return the size of the given shared object
    // RETURN:
    //	a) If found, return size of shared object
    //	b) Else, return E_SHARED_MEM_NOT_EXISTS
    //
    struct Share *ptr_share = find_share(ownerID, shareName);
    if (ptr_share == NULL)
        return E_SHARED_MEM_NOT_EXISTS;
    else
        return ptr_share->size;

    return 0;
}
//===========================================================

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
// Allocates a new shared object and initialize its member
// It dynamically creates the "framesStorage"
// Return: allocatedObject (pointer to struct Share) passed by reference
struct Share *alloc_share(int32 ownerID, char *shareName, uint32 size, uint8 isWritable)
{
    // TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
    // Your code is here
    // Comment the following line
    //	panic("alloc_share() is not implemented yet...!!");

    //	Allocate a new shared object
    //	Initialize its members:
    //	references = 1,
    //	ID = VA of created object after masking-out its most significant bit
    //	Create the "framesStorage“: array of pointers to struct FrameInfo to save pointer(s) to the shared frame(s)
    //	Initialize it by ZEROs
    //	Return:
    //	 If succeed: pointer to the created object for struct Share
    //	If failed: UNDO any allocation & return NULL

    //	uint32* alloc_VA =kmalloc(size);
    //	if(!alloc_VA){
    //		return NULL;
    //	}

    struct Share *obj = (struct Share *)kmalloc(sizeof(struct Share));
    if (!obj)
    {
        return NULL;
    }
    obj->ID = 0;
    obj->references = 1;
    obj->ownerID = ownerID;
    //	obj->name=shareName;
    strncpy(obj->name, shareName, 64);
    obj->size = size;
    obj->isWritable = isWritable;

    int frames_num = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    obj->framesStorage = (struct FrameInfo **)kmalloc(frames_num * sizeof(struct FrameInfo *));
    // check allocation
    if (!obj->framesStorage)
    {
        kfree(obj);
        return NULL;
    }
    // init frames
    for (int i = 0; i < frames_num; i++)
    {
        obj->framesStorage[i] = NULL;
    }
    return obj;
}

//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char *shareName, uint32 size, uint8 isWritable, void *virtual_address)
{
    // TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
    // Your code is here
    // Comment the following line
    //	panic("create_shared_object() is not implemented yet...!!");

    struct Env *myenv = get_cpu_proc(); // The calling environment

    // This function should create the shared object at the given virtual address with the given size
    // and return the ShareObjectID
    // RETURN:
    //	a) ID of the shared object (its VA after masking out its msb) if success
    //	b) E_SHARED_MEM_EXISTS if the shared object already exists
    //	c) E_NO_SHARE if failed to create a shared object

    //	Allocate & Initialize a new share object
    //	Add it t
    //=====
    //	Allocate ALL required space in the physical memory on a PAGE boundary
    //	Map them on the given "virtual_address" on the current process with WRITABLE permissions
    //	Add each allocated frame to "frames_storage" of this shared object to keep track of them for later use

    //	RETURN:
    //		ID of the shared object (its VA after masking out its msb) if success
    //		E_SHARED_MEM_EXISTS if the shared object already exists
    //		E_NO_SHARE if failed to create a shared object
    //=================
    //

    //	acquire_kspinlock(&AllShares.shareslock);
    struct Share *exist_shared_obj = find_share(ownerID, shareName);
    //	release_kspinlock(&AllShares.shareslock);

    if (exist_shared_obj)
    {
        //		cprintf("here");
        return E_SHARED_MEM_EXISTS;
    }
    //	cprintf("out of if e_shared_mem_exists");
    struct Share *shared_obj = alloc_share(ownerID, shareName, size, isWritable);

    if (!shared_obj)
    {
        return E_NO_SHARE;
    }
    // MASK VA
    shared_obj->ID = (((uint32)virtual_address) << 1) >> 1;

    uint32 frames_num = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    uint32 current_VA = (uint32)virtual_address;

    for (uint32 i = 0; i < frames_num; i++)
    {
        struct FrameInfo *frame;
        int ret = allocate_frame(&frame);
        // RETURNS
        //   0 -- on success
        //   If failed, it panic.
        if (ret != 0)
        {
            for (uint32 j = 0; j < i; j++)
            {
                unmap_frame(myenv->env_page_directory, ((uint32)virtual_address + j * PAGE_SIZE));
                free_frame(shared_obj->framesStorage[j]);
            }
            kfree(shared_obj->framesStorage);
            kfree(shared_obj);
            return E_NO_SHARE;
        }

        shared_obj->framesStorage[i] = frame;
        uint32 *ptr_page_table;
        get_page_table(myenv->env_page_directory, current_VA, &ptr_page_table);

        // first one creat this page will be the owner he will have all permations
        int owner_perm = PERM_USER | PERM_UHPAGE | PERM_WRITEABLE;

        int ret_map = map_frame(myenv->env_page_directory, frame, current_VA, owner_perm);
        // RETURNS:
        //   0 on success
        if (ret_map != 0)
        {
            // if maping failed
            // undo
            free_frame(frame);
            for (uint32 j = 0; j < i; j++)
            {
                unmap_frame(myenv->env_page_directory, ((uint32)virtual_address + j * PAGE_SIZE));
                free_frame(shared_obj->framesStorage[j]);
            }
            kfree(shared_obj->framesStorage);
            kfree(shared_obj);
            return E_NO_SHARE;
        }
        // go to next page
        current_VA += PAGE_SIZE;
    }

    acquire_kspinlock(&AllShares.shareslock);
    LIST_INSERT_TAIL(&AllShares.shares_list, shared_obj);
    release_kspinlock(&AllShares.shareslock);

    return shared_obj->ID;
}

//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char *shareName, void *virtual_address)
{
    // TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
    // Your code is here
    // Comment the following line
    //	panic("get_shared_object() is not implemented yet...!!");

    struct Env *myenv = get_cpu_proc(); // The calling environment

    // 	This function should share the required object in the heap of the current environment
    //	starting from the given virtual_address with the specified permissions of the object: read_only/writable
    // 	and return the ShareObjectID
    // RETURN:
    //	a) ID of the shared object (its VA after masking out its msb) if success
    //	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists
    //======================
    //	Get the shared object from the "shares_list"
    //	Get its physical frames from the
    //	Share these frames with the current process starting from the given "virtual_address"
    //	Use the flag isWritable to make the sharing either read-only OR writable
    //	Update references
    //	RETURN:
    //		ID of the shared object (its VA after masking out its msb) if success
    //		E_SHARED_MEM_NOT_EXISTS if the shared object is NOT exists
    //===================

    acquire_kspinlock(&AllShares.shareslock);
    struct Share *shared_obj = find_share(ownerID, shareName);
    if (!shared_obj)
    {
        release_kspinlock(&AllShares.shareslock);
        return E_SHARED_MEM_NOT_EXISTS;
    }

    uint32 frames_num = ROUNDUP(shared_obj->size, PAGE_SIZE) / PAGE_SIZE;
    uint32 current_VA = (uint32)virtual_address;

    for (uint32 i = 0; i < frames_num; i++)
    {
        //	    struct FrameInfo* frame = shared_obj->framesStorage[i];

        uint32 *ptr_page_table;
        get_page_table(myenv->env_page_directory, current_VA, &ptr_page_table);

        int basic_perm = PERM_USER | PERM_UHPAGE;
        // check isWritabe permission
        if (shared_obj->isWritable)
        {
            basic_perm = PERM_USER | PERM_UHPAGE | PERM_WRITEABLE;
        }
        int map_ret = map_frame(myenv->env_page_directory, shared_obj->framesStorage[i], current_VA, basic_perm);
        // lw m3riftsh a map h unmap le7ad el frame ally wakf 3ndh
        //	    if (map_ret != 0) {
        //	        for (uint32 j = 0; j < i; j++) {
        //	        unmap_frame(myenv->env_page_directory,((uint32)virtual_address + (j * PAGE_SIZE)));
        //	        }
        //	        release_kspinlock(&AllShares.shareslock);
        //	        return E_NO_SHARE;
        //	    }
        current_VA += PAGE_SIZE;
    }

    // update references
    shared_obj->references += 1;
    int share_ID = shared_obj->ID;
    release_kspinlock(&AllShares.shareslock);

    return share_ID;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
// delete the given shared object from the "shares_list"
// it should free its framesStorage and the share object itself
void free_share(struct Share *ptrShare)
{
    // TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
    // Your code is here
    // Comment the following line
    panic("free_share() is not implemented yet...!!");
}

//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
    // TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
    // Your code is here
    // Comment the following line
    panic("delete_shared_object() is not implemented yet...!!");

    struct Env *myenv = get_cpu_proc(); // The calling environment

    // This function should free (delete) the shared object from the User Heapof the current environment
    // If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
    // RETURN:
    //	a) 0 if success
    //	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

    // Steps:
    //	1) Get the shared object from the "shares" array (use get_share_object_ID())
    //	2) Unmap it from the current environment "myenv"
    //	3) If one or more table becomes empty, remove it
    //	4) Update references
    //	5) If this is the last share, delete the share object (use free_share())
    //	6) Flush the cache "tlbflush()"
}
