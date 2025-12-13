/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}
/*2024*/ void setFASTNchanceCLOCK(bool fast){ FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL(){ _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL(){if(_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0) ;
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;
extern uint32 sys_calculate_free_frames() ;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter ++ ;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{

		if (userTrap) {
					/*============================================================================================*/
					//TODO: [PROJECT'24.MS2 - #08] [2] FAULT HANDLER I - Check for invalid pointers
					//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
					//your code is here
					int joee;
					joee = pt_get_page_permissions(faulted_env->env_page_directory,
							fault_va);

					if (fault_va >= USER_LIMIT) {
						env_exit();
					} else if ((joee & PERM_WRITEABLE) || (joee & PERM_PRESENT)) {
						env_exit();
					}

					else if (fault_va >= USER_HEAP_START) {
						if (fault_va < USER_HEAP_MAX) {
							if (!(joee & PERM_AVAILABLE)) {
								env_exit();
							}
						}
					}
					/*============================================================================================*/
				}




//		if (userTrap)
//		        {
//			cprintf("inside usertrap");
//		            /*============================================================================================*/
//		            //TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
//		            //(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
//		            //your code is here
//
//		            // First get page permissions to check what we're dealing with
//		            int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
//		            cprintf("%d",perms);
//		            // If page exists (perms != -1), check for invalid pointers
//		            if(perms != (uint32)-1)
//		            {
//		            	cprintf("perm !=-1");
//		                // Check 1: Pointing to UNMARKED page in user heap
//		                if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
//		                {
//		                    // If page is present but not marked as user heap page (PERM_UHPAGE)
//		                    if(!(perms & PERM_UHPAGE))
//		                    {
//		                        cprintf("Invalid Unmarked page at VA %x\n", fault_va);
//			                    env_exit();
//		                    }
//		                }
//
//		                // Check 2: Pointing to kernel address
//		                // Note: This shouldn't happen if page exists in user space,
//		                // but we check it anyway for safety
//		                if(fault_va >= KERNEL_BASE)
//		                {
//		                    cprintf("Invalid pointed to kernel at VA %x\n", fault_va);
//		                    env_exit();
//		                }
//
//		                // Check 3: Page exists with read-only permissions when trying to write
//		                // Check if this is a write fault (check error code in trap frame)
//		                // Error code bit 1 indicates write operation (1) or read operation (0)
//		                if(tf->tf_err & 0x2)  // Write operation
//		                {
//		                    // If page is read-only and we're trying to write
//		                    if(!(perms & PERM_WRITEABLE))
//		                    {
//		                        cprintf("Invalid write to read-only page at VA %x\n", fault_va);
//			                    env_exit();
//		                    }
//		                }
//		            }
//		            else
//		            {
//		                // Page doesn't exist, check if it's pointing to kernel space
//		                // This handles the case where the page table entry doesn't exist
//		                // but the address is in kernel space
//		                if(fault_va >= KERNEL_BASE)
//		                {
//		                    cprintf("Invalid pointed to kernel at VA %x\n", fault_va);
//		                    env_exit();
//		                }
//		            }
//		        }
//


//		if (userTrap)
//		{
//			/*============================================================================================*/
//						//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
//						//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
//						//your code is here
//
//			uint32 page_permissions = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
//			cprintf("%x",page_permissions);
//						// 1. Pointing to UNMARKED page in user heap
//						// Correction: Use < and &&, and check against -1
//						if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
//						{
//							// Check if page exists (not -1) AND (it is present) AND (NOT UHPAGE)
//							if(page_permissions != -1 && (page_permissions & PERM_PRESENT) && !(page_permissions & PERM_UHPAGE))
//							{
//								cprintf( "Invalid Unmarked page at VA %x \n", fault_va);
//								env_exit();
//							}
//						}
//
//						// 2. Pointing to kernel
//						if(fault_va >= KERNEL_BASE)
//						{
//							cprintf( "Invalid pointed to kernel at VA %x \n", fault_va);
//							env_exit();
//						}
//
//						// 3. Exist with read-only permissions (Attempted Write)
//						// Check if page exists (not -1) AND (it is present) AND (NOT WRITEABLE)
//						if(page_permissions != -1 && (page_permissions & PERM_PRESENT) && !(page_permissions & PERM_WRITEABLE))
//						{
//							cprintf( "Invalid read-only at VA %x \n", fault_va);
//							env_exit();
//						}
//						//YYY
//		}
			/*============================================================================================*/

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}


//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize, struct PageRef_List *pageReferences)
{
 //TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
 //Your code is here
 //Comment the following line
 //panic("get_optimal_num_faults() is not implemented yet...!!");
     int faults = 0;
     uint32 wsArray[maxWSSize];
     int wsCount = 0;
     struct WorkingSetElement *wse;
     LIST_FOREACH(wse, initWorkingSet)
     {
         if (wsCount < maxWSSize)
             wsArray[wsCount++] = ROUNDDOWN(wse->virtual_address, PAGE_SIZE);
     }
     struct PageRefElement *pref;
     LIST_FOREACH(pref, pageReferences)
     {
         uint32 va_page = ROUNDDOWN(pref->virtual_address, PAGE_SIZE);
         int hit = 0;
         for (int i = 0; i < wsCount; i++)
         {
             if (wsArray[i] == va_page)
             {
                 hit = 1;
                 break;
             }
         }
         if (hit) continue;
         faults++;
         if (wsCount < maxWSSize)
         {
             wsArray[wsCount++] = va_page;
         }
         else
         {
             int victimIndex = -1;
             int farthestUse = -1;
             for (int i = 0; i < wsCount; i++)
             {
                 int found = 0;
                 int distance = 0;
                 struct PageRefElement *future = LIST_NEXT(pref);
                 while (future != NULL)
                 {
                     distance++;
                     uint32 future_va = ROUNDDOWN(future->virtual_address, PAGE_SIZE);
                     if (future_va == wsArray[i])
                     {
                         found = 1;
                         break;
                     }
                     future = LIST_NEXT(future);
                 }
                 if (!found)
                 {
                     victimIndex = i;
                     break;
                 }
                 else if (distance > farthestUse)
                 {
                     farthestUse = distance;
                     victimIndex = i;
                 }
             }

             wsArray[victimIndex] = va_page;
         }
     }

     return faults;
}


void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	 {
	  //TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
	  //Your code is here
	  //Comment the following line
	      uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);
	      struct FrameInfo *ptr_frame_info;
	      uint32 *ptr_page_table;
	      ptr_frame_info = get_frame_info(faulted_env->env_page_directory, va_page, &ptr_page_table);

	      if (ptr_frame_info != NULL)
	      {
	      }
	      else
	      {
	          if (LIST_SIZE(&(faulted_env->page_WS_list)) >= faulted_env->page_WS_max_size)
	          {
	              struct WorkingSetElement *wse = LIST_FIRST(&(faulted_env->page_WS_list));
	              while (wse != NULL)
	              {
	                  struct WorkingSetElement *next_wse = LIST_NEXT(wse);
	                  unmap_frame(faulted_env->env_page_directory, wse->virtual_address);
	                  LIST_REMOVE(&(faulted_env->page_WS_list), wse);
	                  kfree(wse);
	                  wse = next_wse;
	              }
	          }
	          struct FrameInfo *finfo = NULL;
	          int ret_alloc = allocate_frame(&finfo);
	          if (ret_alloc != 0)
	          { panic("OPTIMAL: allocate_frame failed");}
	          map_frame(faulted_env->env_page_directory, finfo, va_page, PERM_USER | PERM_WRITEABLE);
	          int r = pf_read_env_page(faulted_env, (void*)va_page);
	          if (r == E_PAGE_NOT_EXIST_IN_PF)
	          {
	              int is_stack = (va_page >= USTACKBOTTOM) && (va_page < USTACKTOP);
	              int is_heap  = (va_page >= USER_HEAP_START) && (va_page < USER_HEAP_MAX);
	              if (!(is_stack || is_heap))
	                  env_exit();
	          }
	          struct WorkingSetElement *wse_new = env_page_ws_list_create_element(faulted_env, va_page);
	          LIST_INSERT_TAIL(&(faulted_env->page_WS_list), wse_new);
	          if (LIST_SIZE(&(faulted_env->page_WS_list)) == faulted_env->page_WS_max_size)
	              faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
	          else
	              faulted_env->page_last_WS_element = NULL;
	      }
	      struct PageRefElement *pref_new = kmalloc(sizeof(struct PageRefElement));
	      if (pref_new == NULL){
	          panic("ERROR: Out of kernel heap space for PageRefElement");}
	      pref_new->virtual_address = va_page;
	      LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), pref_new);

	 }
	else
	{
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if(wsSize < (faulted_env->page_WS_max_size))
			{
				//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
				struct FrameInfo* finfo=NULL;
				int ret = allocate_frame(&finfo);
				map_frame(faulted_env->env_page_directory,finfo,fault_va,PERM_USER|PERM_WRITEABLE);
				if(ret != 0){
					return;
				}
				else{
					int ret2 = pf_read_env_page(faulted_env,(void *)fault_va);
					if (ret2 == E_PAGE_NOT_EXIST_IN_PF)
					{
						int is_stack = (fault_va >= USTACKBOTTOM) && (fault_va < USTACKTOP);
						int is_heap = (fault_va >= USER_HEAP_START) && (fault_va < USER_HEAP_MAX);
						if(is_stack || is_heap)
						{}
						else
						{
							env_exit();
						}
					}
				}
				struct WorkingSetElement *new_wse = env_page_ws_list_create_element(faulted_env,fault_va);
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list),new_wse);
				if (LIST_SIZE(&faulted_env->page_WS_list)== faulted_env->page_WS_max_size)
				{
					faulted_env->page_last_WS_element = LIST_FIRST(
					&faulted_env->page_WS_list);
				}
				else {
					faulted_env->page_last_WS_element = NULL;
				}
			}

		else
		{
			if (isPageReplacmentAlgorithmCLOCK())
			   {
			       //TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
			       //Your code is here
			       //Comment the following line
			       //panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			    uint32 va_page = ROUNDDOWN(fault_va, PAGE_SIZE);

			    struct WorkingSetElement *victimWSElement = faulted_env->page_last_WS_element;
			    if (victimWSElement == NULL)
			        victimWSElement = LIST_FIRST(&(faulted_env->page_WS_list));

			    while (1)
			    {
			        uint32 perms = pt_get_page_permissions(faulted_env->env_page_directory, victimWSElement->virtual_address);
			        if (perms & PERM_USED)
			        {
			            pt_set_page_permissions(faulted_env->env_page_directory, victimWSElement->virtual_address, 0, PERM_USED);
			            victimWSElement = LIST_NEXT(victimWSElement);
			            if (victimWSElement == NULL)
			                victimWSElement = LIST_FIRST(&(faulted_env->page_WS_list));
			        }
			        else
			        {
			            break;
			        }
			    }

			    uint32 victim_va = ROUNDDOWN(victimWSElement->virtual_address, PAGE_SIZE);
			    uint32 victim_perms = pt_get_page_permissions(faulted_env->env_page_directory, victim_va);

			    if (victim_perms & PERM_MODIFIED)
			    {
			        struct FrameInfo* victim_frame = get_frame_info(faulted_env->env_page_directory, victim_va, NULL);
			        if (victim_frame != NULL)
			        {
			            pf_update_env_page(faulted_env, victim_va, victim_frame);
			        }
			    }

			    unmap_frame(faulted_env->env_page_directory, victim_va);
			    LIST_REMOVE(&(faulted_env->page_WS_list), victimWSElement);
			    kfree(victimWSElement);

			    struct FrameInfo *finfo = NULL;
			    int ret_alloc = allocate_frame(&finfo);
			    if (ret_alloc != 0)
			        panic("CLOCK: allocate_frame failed");

			    map_frame(faulted_env->env_page_directory, finfo, va_page, PERM_USER | PERM_WRITEABLE);

			    int r = pf_read_env_page(faulted_env, (void*)va_page);
			    if (r == E_PAGE_NOT_EXIST_IN_PF)
			    {
			        int is_stack = (va_page >= USTACKBOTTOM) && (va_page < USTACKTOP);
			        int is_heap  = (va_page >= USER_HEAP_START) && (va_page < USER_HEAP_MAX);
			        if (!(is_stack || is_heap))
			            env_exit();
			    }

			    struct WorkingSetElement *new_wse = env_page_ws_list_create_element(faulted_env, va_page);
			    LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_wse);

			    faulted_env->page_last_WS_element = LIST_NEXT(new_wse);
			    if (faulted_env->page_last_WS_element == NULL)
			        faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));

			   }
			  else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			      {
			          //TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
			          //Your code is here
			          //Comment the following line
			          //panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			          //LRU AGING CODE
			          struct WorkingSetElement * lru_victim = NULL;
			          uint32 min_time_stamp =0xFFFFFFFF;
			          struct WorkingSetElement * cur_element ;
			          // SEARCH  FOR  VICTIM WITH LOWEST TIME_STAMP
			          LIST_FOREACH_SAFE(cur_element,&faulted_env->page_WS_list,WorkingSetElement)
			            {
			              if(lru_victim == NULL || cur_element->time_stamp<min_time_stamp)
			                {
			                    min_time_stamp = cur_element->time_stamp;
			                    lru_victim =cur_element;
			                }
			            }
			          if (lru_victim == NULL)
			          {
			            panic("LRU failed to found victim ");
			          }
			          uint32 victim_addr = lru_victim->virtual_address ;
			          // ready to remove it from WS
			          LIST_REMOVE(&faulted_env->page_WS_list,lru_victim);
			          unmap_frame(faulted_env->env_page_directory,victim_addr);
			          kfree(lru_victim);
			          int ret = alloc_page(faulted_env->env_page_directory,fault_va,PERM_USER|PERM_PRESENT|PERM_WRITEABLE,0);
			          if(ret != 0)
			          {
			            panic("failed to allocated page");
			          }
			          //READ FAULTED PAGE FROM PAGE FILE TO MEM
			          pf_read_env_page(faulted_env,(void *)fault_va);
			          // CREATE WS_ELEMENT AND ADD IT TO WS_LIST
			          struct WorkingSetElement * faulted_page = env_page_ws_list_create_element(faulted_env,fault_va);
			          LIST_INSERT_TAIL(&faulted_env->page_WS_list,faulted_page);
			          // UPDATE page_last_WS_element AFTER REPLACEMENT
			          faulted_env->page_last_WS_element = faulted_page;


			        }
			    else if (isPageReplacmentAlgorithmModifiedCLOCK())
			        {
			          //TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
			          //Your code is here
			          //Comment the following line
			          //panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			          struct WorkingSetElement * modi_victim = NULL;
			          struct WorkingSetElement * cur_element ;
			          bool isfound = 0;
			          // TRY1 :  SEARCH FOR BEST_VICTIM
			          LIST_FOREACH_SAFE(cur_element,&faulted_env->page_WS_list,WorkingSetElement)
			          {    uint32 prems = pt_get_page_permissions(faulted_env->env_page_directory,cur_element->virtual_address);
			              int used_bit = (prems&PERM_USED) != 0;
			              int modified_bit = (prems&PERM_MODIFIED) !=0;
			              if(used_bit ==0 &&modified_bit == 0)
			              {
			                modi_victim = cur_element;
			                isfound =1;
			                break;
			              }
			          }
			          if(!isfound)
			        {
			            //TRY2: SEARCH FOR USED_BIT =0 AND IF NOT FOUND LOOP IN WSE LIST FOR  SEARCH TEY1
			          for(int i =0 ;i<2;i++)
			          {
			            LIST_FOREACH_SAFE(cur_element,&faulted_env->page_WS_list,WorkingSetElement)
			                {
			                  uint32 prems = pt_get_page_permissions(faulted_env->env_page_directory,cur_element->virtual_address);
			                  int used_bit = (prems&PERM_USED) != 0;
			                  if(used_bit == 0)
			                  {
			                    modi_victim =cur_element;
			                    isfound=1;
			                    break;
			                  }
			                  else if (used_bit ==1)
			                  {
			                    // clear used bit
			                    pt_set_page_permissions(faulted_env->env_page_directory,cur_element->virtual_address, 0,PERM_USED );
			                  }
			                }
			            // if found best victim after second   iteration
			            if(isfound)
			            {break;}
			          }

			}
			          if(modi_victim ==NULL)
			          {
			            panic(" MODIFIED CLOCK failed to found victim ");
			          }
			          uint32 victim_addr =modi_victim->virtual_address;
			          //CHECK IF VICTIM PAGE IS MODIFY
			          uint32 ret = pt_get_page_permissions(faulted_env->env_page_directory,victim_addr);
			          if(ret&& PERM_MODIFIED !=0)
			          {
			            //is dirty
			            uint32 * ptr_table;
			            struct FrameInfo *ptr_victim_info = get_frame_info(faulted_env->env_page_directory,victim_addr,&ptr_table);
			            int result=pf_update_env_page(faulted_env,victim_addr,ptr_victim_info);
			            if (result == E_NO_PAGE_FILE_SPACE)
			            {
			              panic(" the page file is full, can’t add any more pages to it.");
			            }
			          } // ready to remove it from WS
			          LIST_REMOVE(&faulted_env->page_WS_list,modi_victim);
			          unmap_frame(faulted_env->env_page_directory,victim_addr);
			          kfree(modi_victim);
			          int allocated = alloc_page(faulted_env->env_page_directory,fault_va,PERM_USER|PERM_PRESENT|PERM_WRITEABLE,0);
			          if(allocated != 0)
			          {

			          }
			          //READ FAULTED PAGE FROM PAGE FILE TO MEM
			          pf_read_env_page(faulted_env,(void *)fault_va);
			          // CREATE WS_ELEMENT AND ADD IT TO WS_LIST
			          struct WorkingSetElement * faulted_page = env_page_ws_list_create_element(faulted_env,fault_va);
			          LIST_INSERT_TAIL(&faulted_env->page_WS_list,faulted_page);
			          // UPDATE page_last_WS_element AFTER REPLACEMENT
			          faulted_env->page_last_WS_element = faulted_page;
			        }
			      }
			    }
#endif
}


void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}

