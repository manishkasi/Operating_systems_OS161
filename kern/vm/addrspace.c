/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <mips/tlb.h>
#include <proc.h>
#include <spl.h>

#define STACK_SIZE 1024
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as = NULL;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->as_segment = NULL;
	as->as_stackpointer = USERSTACK;
	as->as_heap_start_pointer = (vaddr_t)0;
	as->as_heap_end_pointer = (vaddr_t)0;
	as->as_pte_start = NULL;
	//as->as_pte_end = NULL;

	return as;
}


int
as_copy(struct addrspace *old, struct addrspace **ret)
{
  	/*(void) old;
	(void) ret;
	return 0; */
	
    	struct addrspace *newas = NULL;

        newas = as_create();
        if (newas==NULL) {
                return ENOMEM;
        }

        /*
         * Write this.
        */
        struct segment *temp1 = old->as_segment;
        struct segment *temp2 = newas->as_segment;
        struct segment *temp3 = NULL;
        if(temp1 != NULL){
                if(temp2 == NULL){
                        temp2 = kmalloc(sizeof(struct segment*));
                        temp2->as_vbase = temp1->as_vbase;
                        temp2->as_npages = temp1->as_npages;
                        temp2->permission = temp1->permission;
                        temp2->next_segment = NULL;
                        newas->as_segment = temp2;
                }
        
        while(temp1->next_segment != NULL){
                temp1 = temp1->next_segment;
                temp3 = kmalloc(sizeof(struct segment*));
                temp3->as_vbase = temp1->as_vbase;
                temp3->as_npages = temp1->as_npages;
                temp3->permission = temp1->permission;
                temp3->next_segment = NULL;
                temp2->next_segment = temp3;
                temp2 = temp2->next_segment;
        }
	}

        newas->as_stackpointer = old->as_stackpointer;
        newas->as_heap_start_pointer = old->as_heap_start_pointer;
        newas->as_heap_end_pointer = old->as_heap_end_pointer;

        struct page_table_entry *old_as_pte_temp = old->as_pte_start;

        while(old_as_pte_temp != NULL){
                struct page_table_entry *new_as_pte = NULL;
		new_as_pte = kmalloc(sizeof(struct page_table_entry*));
                new_as_pte->v_pn = old_as_pte_temp->v_pn;
                paddr_t new_p_pn = alloc_ppages(new_as_pte);
                if(new_p_pn){
                        new_as_pte->p_pn = new_p_pn;
                }
                else{
                        return EFAULT;
                }
                memmove((void *)PADDR_TO_KVADDR(new_as_pte->p_pn),
                        (const void *)PADDR_TO_KVADDR(old_as_pte_temp->p_pn),
                        PAGE_SIZE);
                new_as_pte->state = 1;
                new_as_pte->next_pte = NULL;
                /*if(newas->as_pte_start == NULL){
                         newas->as_pte_start = newas->as_pte_end = new_as_pte;
                }
                else{
                        newas->as_pte_end->next_pte = new_as_pte;
                        newas->as_pte_end = new_as_pte;
                }*/
		if(newas->as_pte_start == NULL){
                        newas->as_pte_start = new_as_pte;
                }else{
                        new_as_pte->next_pte = newas->as_pte_start;
                        newas->as_pte_start = new_as_pte;
                }

                old_as_pte_temp = old_as_pte_temp->next_pte;
        }

        *ret = newas;
        return 0;
}

void
as_destroy(struct addrspace *as)
{
        /*
         * Clean up as needed.
         */

        /*int spl = splhigh(); //from as_activate

        for (int i=0; i<NUM_TLB; i++) {
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);*/

        struct segment *temp_seg = NULL;
	temp_seg = as->as_segment;
        struct segment *temp2_seg = NULL;
        while(temp_seg != NULL){
                temp2_seg = temp_seg;
                temp_seg = temp_seg->next_segment;
                kfree(temp2_seg);
		temp2_seg = NULL;
        }
        struct page_table_entry *temp_pte = NULL;
	temp_pte = as->as_pte_start;
        struct page_table_entry *temp2_pte = NULL;
        while(temp_pte != NULL){
                temp2_pte = temp_pte;
                temp_pte = temp_pte->next_pte;
                free_ppages(temp2_pte);
                kfree(temp2_pte);
		temp2_pte = NULL;
        }
	
	as->as_stackpointer = 0;
        as->as_heap_start_pointer = 0;
        as->as_heap_end_pointer = 0;
	as->as_pte_start = 0; 
        kfree(as);
}

void
as_activate(void)
{
        int i, spl;
        struct addrspace *as = NULL;

        as = proc_getas();
        if (as == NULL) {
                return;
        }

        /* Disable interrupts on this CPU while frobbing the TLB. */
        spl = splhigh();

        for (i=0; i<NUM_TLB; i++) {
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);
}


void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	//(void)as;
	//(void)vaddr;
	//(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;

	size_t npages;

        /* Align the region. First, the base... */
        memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
        vaddr &= PAGE_FRAME;

        /* ...and now the length. */
        memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

        npages = memsize / PAGE_SIZE;

	struct segment *temp_seg = NULL;
	temp_seg = as->as_segment;
	while(temp_seg != NULL){
		temp_seg = temp_seg->next_segment;
	}		

	temp_seg = kmalloc(sizeof(struct segment *));
	if(temp_seg == NULL){
		return ENOMEM;
	}
	temp_seg->as_vbase = vaddr;
	temp_seg->as_npages = npages;
	temp_seg->permission = 7;
	
	temp_seg->next_segment = NULL;
       	if(as->as_segment == NULL){
               as->as_segment = temp_seg;
       	}
       	else{
               struct segment *seg_temp = NULL;
		seg_temp = as->as_segment;
               while(seg_temp->next_segment != NULL){
                       seg_temp = seg_temp->next_segment;
               }
               seg_temp->next_segment = temp_seg;
        }
	as->as_heap_start_pointer = (vaddr + npages * PAGE_SIZE);
	//as->as_heap_end_pointer = as->as_heap_start_pointer;
	as->as_heap_end_pointer = (vaddr + npages * PAGE_SIZE);
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	as->as_stackpointer = USERSTACK;
	
	return 0;
}

