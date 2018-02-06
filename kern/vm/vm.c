#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>
#include <stat.h>
#include <bitmap.h>
#include <kern/fcntl.h>
#include <uio.h>


//paddr_t coremap_start_addr;
//int no_of_pages; 

static struct spinlock vm_spinlock = SPINLOCK_INITIALIZER;

void
coremap_init(){
	struct coremap* coremap_ptr;
	paddr_t ram_size = ram_getsize();
	no_of_pages = ram_size/PAGE_SIZE;	
	first_free = ram_getfirstfree();
	coremap_start_addr = first_free;
	size_of_coremap = sizeof(struct coremap) * no_of_pages;
	first_free += size_of_coremap;
	
	if(first_free % PAGE_SIZE != 0){
		first_free = ((first_free/PAGE_SIZE)+1)*PAGE_SIZE;
	}

	//bytes_in_use += first_free;
	bytes_in_use = 0;	

	coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
	// allocate coremap struct size from 0 to first_free
	for(unsigned int i=0; i < first_free/PAGE_SIZE; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
		coremap_ptr->state = -1; //fixed
		coremap_ptr->chunk_size = 1;
		coremap_ptr->corr_pte = NULL;
	}

	// allocate coremap struct size from first_free to total RAM size.
	for(int i = first_free/PAGE_SIZE; i < no_of_pages; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
		coremap_ptr->state = 0; //free
                coremap_ptr->chunk_size = 0;
		coremap_ptr->corr_pte = NULL;
	}
}


vaddr_t
alloc_kpages(unsigned pages){
	
	if(swap_enabled){

		struct coremap * coremap_ptr;
		//spinlock_acquire(&vm_spinlock);
		coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
		unsigned contiguous_pages = 0;
		int start_of_free_page=0;
		for(int i = 0; i < no_of_pages; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
			if(coremap_ptr->state == 0){
				contiguous_pages++;
			}else{
				contiguous_pages=0;
			}
			if(contiguous_pages == pages){
				start_of_free_page = i-pages+1;
				break;
			}
		}
		if(start_of_free_page == 0){
			//spinlock_release(&vm_spinlock);
			start_of_free_page = swap_out();
			//spinlock_acquire(&vm_spinlock);
		}
		for(unsigned int j = 0; j < pages; j++){
			coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr+(start_of_free_page+j)*sizeof(struct coremap));
			coremap_ptr->state=1; //fixed
			if(j == 0){
				coremap_ptr->chunk_size = pages;
			}
			coremap_ptr->corr_pte = NULL;
		}	
		bzero((void *) PADDR_TO_KVADDR(start_of_free_page * PAGE_SIZE), PAGE_SIZE * pages);
		bytes_in_use += pages*PAGE_SIZE;
		//spinlock_release(&vm_spinlock);
		return PADDR_TO_KVADDR(start_of_free_page*PAGE_SIZE);
	
	}
	

	else{

		struct coremap *coremap_ptr;
		spinlock_acquire(&vm_spinlock);
		coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
		unsigned contiguous_pages = 0;
		int start_of_free_page = 0;
		for(int i = 0; i < no_of_pages; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
			if(coremap_ptr->state == 0){
				contiguous_pages++;
			}else{
				contiguous_pages=0;
			}
			if(contiguous_pages == pages){
				start_of_free_page = i-pages+1;
				break;
			}
		}
		if(start_of_free_page == 0){
			spinlock_release(&vm_spinlock);
			return 0;
		}
		for(unsigned int j = 0; j < pages; j++){
			coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr+(start_of_free_page+j)*sizeof(struct coremap));
			coremap_ptr->state=1; //fixed
			if(j == 0){
				coremap_ptr->chunk_size = pages;
			}
			coremap_ptr->corr_pte = NULL;
		}	
		bzero((void *) PADDR_TO_KVADDR(start_of_free_page * PAGE_SIZE), PAGE_SIZE * pages);
		bytes_in_use += pages*PAGE_SIZE;
		spinlock_release(&vm_spinlock);
		return PADDR_TO_KVADDR(start_of_free_page*PAGE_SIZE);
	
	}
}

void
free_kpages(vaddr_t addr){
	spinlock_acquire(&vm_spinlock);
	paddr_t start_paddr = addr - MIPS_KSEG0;
	int index = start_paddr/PAGE_SIZE;
	struct coremap * coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr + index*sizeof(struct coremap));
	int chunk_size = coremap_ptr->chunk_size;
	for(int i = 0; i < chunk_size; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
		coremap_ptr->state=0;
		coremap_ptr->chunk_size = 0;
		coremap_ptr->corr_pte = NULL;
	}
	bytes_in_use -= chunk_size*PAGE_SIZE;
	//bzero((void *)addr, (size_t)chunk_size*PAGE_SIZE);
	spinlock_release(&vm_spinlock);
}

void
free_ppages(struct page_table_entry *temp_pte){
	//(void) vaddr;
        //spinlock_acquire(&vm_spinlock);
        //paddr_t paddr = addr - MIPS_KSEG0;
	if(temp_pte->state == 1){

	        int index = temp_pte->p_pn/PAGE_SIZE;
        	struct coremap * coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr + index*sizeof(struct coremap));
        //int chunk_size = coremap_ptr->chunk_size;
        //for(int i = 0; i < chunk_size; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
                coremap_ptr->state=0;
                coremap_ptr->chunk_size = 0;
        //}
        bytes_in_use -= PAGE_SIZE;
	}else{
		bitmap_unmark(swap_bitmap, temp_pte->p_pn/PAGE_SIZE);
	}	
	
        //bzero((void *)vaddr, (size_t)PAGE_SIZE);
        //spinlock_release(&vm_spinlock);
}


unsigned
int
coremap_used_bytes() {

        /* dumbvm doesn't track page allocations. Return 0 so that khu works. */

        return bytes_in_use;
}

void
vm_tlbshootdown(const struct tlbshootdown *ts){
	(void)ts;
}

void 
vm_bootstrap(){
	int flag = 0;
	char *disk_name = kstrdup("lhd0raw:");
	flag = vfs_open(disk_name, O_RDWR, 0, &swap_vnode);
	rr = 0;
	if(!flag){
		struct stat swap_stat;
		VOP_STAT(swap_vnode, &swap_stat);
		int disk_size = swap_stat.st_size;
		unsigned nbits = disk_size/PAGE_SIZE;
		swap_bitmap = bitmap_create(nbits);
		if(swap_bitmap != NULL){
			swap_enabled = 1;
			rr = first_free/PAGE_SIZE;
		}		
	}
	kprintf("Is swap enabled - %d \n Flag for rr is %d \n", swap_enabled, rr);	
}

int
vm_fault(int faulttype, vaddr_t faultaddress){
	(void) faulttype;
	//(void) faultaddress;
	
	if(swap_enabled){
		int exist_in_seg = 0;

		struct segment *temp_seg;
	
		struct addrspace *addr_sp;
		addr_sp = curproc->p_addrspace;
		temp_seg = addr_sp->as_segment;
		while(temp_seg != NULL) {
			if(faultaddress < (temp_seg->as_vbase + (PAGE_SIZE * temp_seg->as_npages)) && 
				faultaddress >= temp_seg->as_vbase){
				exist_in_seg = 1;
				break;
			}
			temp_seg=temp_seg->next_segment;
		}
	
		struct addrspace *temp_as = curproc->p_addrspace; 

		if(!exist_in_seg){
			// check in stack
			if(faultaddress < temp_as->as_stackpointer 
				&& faultaddress >= (temp_as->as_stackpointer - PAGE_SIZE*STACK_SIZE)){
				exist_in_seg = 1;
			}
		}
		if(!exist_in_seg){
			// check in heap
                	if(faultaddress >= temp_as->as_heap_start_pointer
                        	&& faultaddress <= temp_as->as_heap_end_pointer){
                        	exist_in_seg = 1;
	                }
		}
		if(!exist_in_seg){
			return EFAULT;
		}
	
		struct page_table_entry *temp_pte = temp_as->as_pte_start;
		int exist_in_pt = 0;
		int swap_in = 0;	

		while(temp_pte != NULL){
			//kprintf("ANAND - PTE's vpn value : %d \n", temp_pte->v_pn);
			if(temp_pte->v_pn == (faultaddress & PAGE_FRAME)){
				exist_in_pt = 1;
				if(temp_pte->state == 0){ //on disk
					swap_in = 1;
				}
				break;
			}
			temp_pte = temp_pte->next_pte;
		}

		if(exist_in_pt){
		
			if(swap_in){
				temp_pte->p_pn = alloc_ppages(temp_pte);
				swap_in = 0;
				//temp_pte->state = 1; //memory
			}

		}	


		if(!exist_in_pt){
			struct page_table_entry *new_pte = kmalloc(sizeof(struct page_table_entry*));
			if(new_pte == NULL){
				return ENOMEM;
			}
			new_pte->state = 1;
			new_pte->v_pn = faultaddress & PAGE_FRAME;
			new_pte->next_pte = NULL;	
			paddr_t new_p_pn = alloc_ppages(new_pte);
			if(new_p_pn){
				new_pte->p_pn = new_p_pn;
			}	

			/*if(temp_as->as_pte_start == NULL || temp_as->as_pte_end == NULL){
				temp_as->as_pte_start = temp_as->as_pte_end = new_pte;
			}else {
				temp_as->as_pte_end->next_pte = new_pte;
				temp_as->as_pte_end = new_pte;
			}*/
			if(temp_as->as_pte_start == NULL){
				temp_as->as_pte_start = new_pte;
			}else{
				new_pte->next_pte = temp_as->as_pte_start;
				temp_as->as_pte_start = new_pte;
			}

			temp_pte = new_pte;
		}
	
		int spl = splhigh();
		if(temp_pte->state){
			int index = tlb_probe(temp_pte->v_pn, 0);
			if(index < 0){
				tlb_random(temp_pte->v_pn, temp_pte->p_pn | TLBLO_DIRTY | TLBLO_VALID);
			}else{
				tlb_write(temp_pte->v_pn, temp_pte->p_pn | TLBLO_DIRTY | TLBLO_VALID, index);
			}
		}
		splx(spl);
		return 0;
	}

	else{

		int exist_in_seg = 0;
	
		struct segment *temp_seg;
	
		struct addrspace *addr_sp;
		addr_sp = curproc->p_addrspace;
		temp_seg = addr_sp->as_segment;
		while(temp_seg != NULL) {
			if(faultaddress < (temp_seg->as_vbase + (PAGE_SIZE * temp_seg->as_npages)) && 
				faultaddress >= temp_seg->as_vbase){
				exist_in_seg = 1;
				break;
			}
			temp_seg=temp_seg->next_segment;
		}
	
		struct addrspace *temp_as = curproc->p_addrspace; 

		if(!exist_in_seg){
			// check in stack
			if(faultaddress < temp_as->as_stackpointer 
				&& faultaddress >= (temp_as->as_stackpointer - PAGE_SIZE*STACK_SIZE)){
				exist_in_seg = 1;
			}
		}
		if(!exist_in_seg){
			// check in heap
                	if(faultaddress >= temp_as->as_heap_start_pointer
                        	&& faultaddress <= temp_as->as_heap_end_pointer){
                        	exist_in_seg = 1;
                	}
		}
		if(!exist_in_seg){
			return EFAULT;
		}
	
		struct page_table_entry *temp_pte = temp_as->as_pte_start;
		int exist_in_pt = 0;	

		while(temp_pte != NULL){
			//kprintf("ANAND - PTE's vpn value : %d \n", temp_pte->v_pn);
			if(temp_pte->v_pn == (faultaddress & PAGE_FRAME)){
				exist_in_pt = 1;
				break;
			}
			temp_pte = temp_pte->next_pte;
		}


		if(!exist_in_pt){
			struct page_table_entry *new_pte = kmalloc(sizeof(struct page_table_entry*));
			if(new_pte == NULL){
				return ENOMEM;
			}
			new_pte->state = 1;
			new_pte->v_pn = faultaddress & PAGE_FRAME;
			new_pte->next_pte = NULL;	
			paddr_t new_p_pn = alloc_ppages(new_pte);
			if(new_p_pn){
				new_pte->p_pn = new_p_pn;
			}	

			/*if(temp_as->as_pte_start == NULL || temp_as->as_pte_end == NULL){
				temp_as->as_pte_start = temp_as->as_pte_end = new_pte;
			}else {
				temp_as->as_pte_end->next_pte = new_pte;
				temp_as->as_pte_end = new_pte;
			}*/
			if(temp_as->as_pte_start == NULL){
				temp_as->as_pte_start = new_pte;
			}else{
				new_pte->next_pte = temp_as->as_pte_start;
				temp_as->as_pte_start = new_pte;
			}
	
			temp_pte = new_pte;
		}
	
		int spl = splhigh();
		if(temp_pte->state){
			int index = tlb_probe(temp_pte->v_pn, 0);
			if(index < 0){
				tlb_random(temp_pte->v_pn, temp_pte->p_pn | TLBLO_DIRTY | TLBLO_VALID);
			}else{
				tlb_write(temp_pte->v_pn, temp_pte->p_pn | TLBLO_DIRTY | TLBLO_VALID, index);
			}
		}
		splx(spl);
		return 0;
	}

}




paddr_t
alloc_ppages(struct page_table_entry *temp_pte){

	if(swap_enabled){
		struct coremap * coremap_ptr;
        	//spinlock_acquire(&vm_spinlock);
       		coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
        	int contiguous_pages = 0;
        	int start_of_free_page=0;
		int pages = 1;
        	for(int i = 0; i < no_of_pages; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
                	if(coremap_ptr->state == 0){
                        	contiguous_pages++;
                	}else{
                        	contiguous_pages=0;
                	}
                	if(contiguous_pages == pages){
                        	start_of_free_page = i-pages+1;
                        	break;
                	}
        	}

		if(start_of_free_page == 0){
		
			start_of_free_page = swap_out();
			if(temp_pte->state == 0){ //swap-in
			
				int temp = temp_pte->p_pn/PAGE_SIZE;
				//spinlock_acquire(&vm_spinlock);
				block_read(temp,start_of_free_page*PAGE_SIZE);
				temp_pte->p_pn = start_of_free_page*PAGE_SIZE;
				//spinlock_release(&vm_spinlock);
				temp_pte->state = 1; //memory
				bitmap_unmark(swap_bitmap, temp);
			}
		}

        	for(int j = 0; j < pages; j++){
                	coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr+(start_of_free_page+j)*sizeof(struct coremap));
                	coremap_ptr->state=2; //dirty
                	if(j == 0){
                        	coremap_ptr->chunk_size = pages;
        		}
			coremap_ptr->corr_pte = temp_pte;
        	}
        	bytes_in_use += pages*PAGE_SIZE;
		//spinlock_release(&vm_spinlock);

		//spinlock_release(&vm_spinlock);
        	return (paddr_t)start_of_free_page*PAGE_SIZE;
	}

	else{
		struct coremap *coremap_ptr;
        	spinlock_acquire(&vm_spinlock);
        	coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
        	int contiguous_pages = 0;
        	int start_of_free_page=0;
		int pages = 1;
        	for(int i = 0; i < no_of_pages; i++, coremap_ptr++){ //coremap_ptr+=sizeof(struct coremap)){
                	if(coremap_ptr->state == 0){
                	        contiguous_pages++;
                	}else{
                        	contiguous_pages=0;
                	}
                	if(contiguous_pages == pages){
                        	start_of_free_page = i-pages+1;
                        	break;
                	}
        	}

	        if(start_of_free_page == 0){
                	spinlock_release(&vm_spinlock);
                	return 0;
        	}

        	for(int j = 0; j < pages; j++){
                	coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr+(start_of_free_page+j)*sizeof(struct coremap));
                	coremap_ptr->state=2; //dirty
                	if(j == 0){
                        	coremap_ptr->chunk_size = pages;
        		}
			coremap_ptr->corr_pte = temp_pte;
        	}
        	bytes_in_use += pages*PAGE_SIZE;
		//spinlock_release(&vm_spinlock);
		bzero((void *)PADDR_TO_KVADDR(start_of_free_page*PAGE_SIZE), (size_t)PAGE_SIZE*pages);
		spinlock_release(&vm_spinlock);
        	return (paddr_t)start_of_free_page*PAGE_SIZE;
	}
}

int
round_robin(){
	
	int x = 0;
	struct coremap *coremap_ptr;
	coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
	while(1){

		coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr+rr*sizeof(struct coremap));
		if(coremap_ptr->state == 2){ //dirty
	//		kprintf("\nRR is %d\n",rr);
			x = rr;
			rr++;
	                if(rr == no_of_pages){
        	                rr = first_free/PAGE_SIZE;
                	}
			break;
		}
		rr++;
		if(rr >= no_of_pages){
			rr = first_free/PAGE_SIZE;
		}
	}	
	return x;

}

void
block_write(int index,paddr_t mem_paddr){

	int success;
	struct iovec swap_iov;
	struct uio swap_u;
	enum uio_rw mode = UIO_WRITE;
	vaddr_t swap_kbase = PADDR_TO_KVADDR(mem_paddr);
	//kprintf("\nSwapping out %ld - %d",(long)swap_kbase,mem_paddr);
	vaddr_t disk_addr = index*PAGE_SIZE;

	swap_iov.iov_kbase = (void *)swap_kbase;
        swap_iov.iov_len = PAGE_SIZE;
        swap_u.uio_iov = &swap_iov;
        swap_u.uio_iovcnt = 1;
        swap_u.uio_offset = disk_addr;
        swap_u.uio_resid = PAGE_SIZE;
        swap_u.uio_segflg = UIO_SYSSPACE;
        swap_u.uio_rw = mode;
        swap_u.uio_space = NULL;

	success = VOP_WRITE(swap_vnode, &swap_u);
	if(!success){
	//	kprintf("\nSwap out success");
	}

}

void
block_read(int index,paddr_t mem_paddr){

        int success;
        struct iovec swap_iov;
        struct uio swap_u;
        enum uio_rw mode = UIO_READ;
        vaddr_t swap_kbase = PADDR_TO_KVADDR(mem_paddr);
	//kprintf("\nSwapping in %ld - %d",(long)swap_kbase,mem_paddr);
	vaddr_t disk_addr = index*PAGE_SIZE;

        swap_iov.iov_kbase = (void *)swap_kbase;
        swap_iov.iov_len = PAGE_SIZE;
        swap_u.uio_iov = &swap_iov;
        swap_u.uio_iovcnt = 1;
        swap_u.uio_offset = disk_addr;
        swap_u.uio_resid = PAGE_SIZE;
        swap_u.uio_segflg = UIO_SYSSPACE;
        swap_u.uio_rw = mode;
        swap_u.uio_space = NULL;

        success = VOP_READ(swap_vnode, &swap_u);
	if(!success){
	//	kprintf("\nSwap in success");
	}

}

int
swap_out(){

	unsigned index;
	int to_be_freed = round_robin();
	struct coremap *coremap_ptr;
	coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr+to_be_freed*sizeof(struct coremap));
	bitmap_alloc(swap_bitmap,&index);
	if(coremap_ptr->corr_pte==NULL)
	{

	//	kprintf("\nSTate is %d to be freed %d\n",coremap_ptr->state,to_be_freed);
	}
	block_write(index,coremap_ptr->corr_pte->p_pn);
	coremap_ptr->corr_pte->p_pn = (paddr_t)index*PAGE_SIZE;
	coremap_ptr->corr_pte->state = 0; //disk

	int spl = splhigh();

	int index_tlb = tlb_probe(coremap_ptr->corr_pte->v_pn, 0);
        if(index_tlb > 0){
		tlb_write(TLBHI_INVALID(index_tlb), TLBLO_INVALID(), index_tlb);
        }
        
	splx(spl);

	coremap_ptr->state = 0; //free
        coremap_ptr->chunk_size = 0;
        coremap_ptr->corr_pte = NULL;
	
	bytes_in_use -= PAGE_SIZE;

	return to_be_freed;

}

void
coremap_values(){
	struct coremap *coremap_ptr;
        coremap_ptr = (struct coremap *) PADDR_TO_KVADDR(coremap_start_addr);
	for(int i=0;i<no_of_pages;i++,coremap_ptr++){
		kprintf("Coremap %d entry state is %d\n",i,coremap_ptr->state);
	}
}

