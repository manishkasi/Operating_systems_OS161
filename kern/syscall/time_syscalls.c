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
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <uio.h>
#include <kern/iovec.h>
#include <vnode.h>
#include <current.h>
#include <test.h>
#include <proc.h>
#include <kern/errno.h>
#include <synch.h>
#include <kern/seek.h>
#include <stat.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <kern/wait.h>
#include <thread.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <vm.h>
#include <spl.h>
#include <mips/tlb.h>

#define MAX_SIZE 65536
char arg_buf[MAX_SIZE];

/*
 * Example system call: get the time of day.
 */
int
sys___time(userptr_t user_seconds_ptr, userptr_t user_nanoseconds_ptr)
{
	struct timespec ts;
	int result;

	gettime(&ts);

	result = copyout(&ts.tv_sec, user_seconds_ptr, sizeof(ts.tv_sec));
	if (result) {
		return result;
	}


	result = copyout(&ts.tv_nsec, user_nanoseconds_ptr,
			 sizeof(ts.tv_nsec));
	if (result) {
		return result;
	}

	return 0;
}

int
sys_read(int fd, userptr_t addr, int length, int *val){
        if(fd < 0 || fd > 63){
                *val = -1;
		return EBADF;
        }
        if((curproc->file_table[fd] == NULL) || ((curproc->file_table[fd]->flag & 3) == O_WRONLY)){
                *val = -1;
		return EBADF;
        }
        struct iovec read_iov;
        struct uio read_uio;
        struct vnode *read_vn = NULL;
        struct filehandle *read_fh = NULL;
        int result = 0;
	
	lock_acquire(curproc->file_table[fd]->fh_lock);		
        read_fh = curproc->file_table[fd];
        read_vn = read_fh->fh_vnode;

        read_iov.iov_ubase = addr;
        read_iov.iov_len = length;

        read_uio.uio_iov = &read_iov;
        read_uio.uio_iovcnt = 1;
        read_uio.uio_resid = length;
        read_uio.uio_offset = read_fh->offset;
        read_uio.uio_segflg = UIO_USERSPACE;
        read_uio.uio_rw = UIO_READ;
        read_uio.uio_space = curproc->p_addrspace;

        result = VOP_READ(read_vn, &read_uio);
        /*if(result){
		*val = -1;
		return result;
	}*/
	*val = length-read_uio.uio_resid; // need to check for the exact bytes written
        read_fh->offset += *val;
	//kprintf_n("ANAND - sys_read , the result after VOP_READ is %d \n", result);

	lock_release(curproc->file_table[fd]->fh_lock);
	return result;
}

int
sys_write(int fd, userptr_t addr, int length, int *val){
	if(fd < 0 || fd > 63){
                return EBADF;
        }
	if((curproc->file_table[fd] == NULL) || (curproc->file_table[fd]->flag & 3) == 0){
                return EBADF;
        }
	struct iovec write_iov;
        struct uio write_uio;
        struct vnode *write_vn = NULL;
        struct filehandle *write_fh = NULL;
        int result = 0;
        //uio_kinit(&write_iov, &write_uio, addr, sizeof(addr), 0, UIO_WRITE);
	lock_acquire(curproc->file_table[fd]->fh_lock);
	
        write_fh = curproc->file_table[fd];
        write_vn = write_fh->fh_vnode;

        write_iov.iov_ubase = addr;
        write_iov.iov_len = length;

        write_uio.uio_iov = &write_iov;
        write_uio.uio_iovcnt = 1;
        write_uio.uio_resid = length;
        write_uio.uio_offset = write_fh->offset;
        write_uio.uio_segflg = UIO_USERSPACE;
        write_uio.uio_rw = UIO_WRITE;
        write_uio.uio_space = curproc->p_addrspace;

        result = VOP_WRITE(write_vn, &write_uio);
	*val = length-write_uio.uio_resid; // need to check for the exact bytes written
        write_fh->offset += *val;	
	
	lock_release(curproc->file_table[fd]->fh_lock);

	return result;
}

int
sys_lseek(int fd, off_t pos, int whence, off_t *val){
	//kprintf_n("ANAND LSEEK 1 : fd is %d, whence is %d, position is %llu  \n", fd, whence, pos);

	if(fd < 0 || fd > 63 || curproc->file_table[fd] == NULL){
                *val = -1;
		return EBADF;
        }
  	//kprintf_n("ANAND LSEEK 2 : fd is %d, whence is %d, position is %llu  \n", fd, whence, pos);	
	if(!VOP_ISSEEKABLE(curproc->file_table[fd]->fh_vnode)){
		*val = -1;
		return ESPIPE;
	}
	

	struct filehandle *fh_lseek = curproc->file_table[fd];
	//kprintf_n("ANAND LSEEK 3 : fd is %d, whence is %d, position is %llu \n", fd, whence, pos);
	//kprintf_n("ANAND LSEEK 3.5 BEFORE seeking : offset is %d \n", fh_lseek->offset);
	off_t lseek_pos = 0;
	struct stat *fh_stat = kmalloc(sizeof(struct stat));	
	//kprintf_n("ANAND LSEEK 6 : before switch - %d - %d  \n", whence, SEEK_END);
	switch(whence){
		case SEEK_SET:
			lseek_pos = pos;
			break;
		
		case SEEK_CUR:
			lseek_pos = fh_lseek->offset + pos;
			break;
		
		case SEEK_END:
                	VOP_STAT(fh_lseek->fh_vnode, fh_stat);
			lseek_pos = fh_stat->st_size + pos;
			break;
		
		default:
			return EINVAL;
	}	
	//kprintf_n("ANAND LSEEK : 10 - fd is %d, whence is %d \n", fd, whence);
	if(lseek_pos < 0){
		return EINVAL;
	}
	fh_lseek->offset = lseek_pos;

	//kfree(fh_stat);
	*val = lseek_pos;
	//kprintf_n("ANAND LSEEK 11 : new seek position is %llu ", lseek_pos);
	return 0;
}

int
sys_open(const char *filename, int flags, int *val){

	char file_name[1024];
        size_t filename_len = 0;
        int result = copyinstr((const_userptr_t)filename, file_name, 1024, &filename_len);
        if (result) {
                *val = -1;
                return result;
        }	
	//char *temp_filename = (char *)filename;
	char *temp_filename = &file_name[0];
	int i = 3;
        int file_desc = -1;
        struct filehandle *fh_open = NULL;

	//struct file
	// check the filename
	if(temp_filename == NULL){
		*val = -1;
		return EFAULT;
	}

	if((temp_filename == (void *)0x40000000) || (temp_filename == (void *)0x80000000)){
                *val = -1;
                return EFAULT;
        }
 
        if(strlen(temp_filename) == 0){
                *val = -1;
                return EINVAL;
        }
 
        if(flags > 63){
                *val = -1;
                return EINVAL;
	}	

	for(; i < 64; i++){
		if( curproc->file_table[i] == NULL) {
			if(file_desc == -1){
				// check for the first empty slot
				file_desc = i;
			}
		}else if(strcmp(curproc->file_table[i]->file_name, temp_filename) 
				&& (curproc->file_table[i]->flag & 3) == (flags & 3)){
			fh_open = curproc->file_table[i];
		}
	}
//	kprintf_n("ANAND OPENTEST 2: %d ", file_desc);
	if(fh_open != NULL){
		curproc->file_table[file_desc] = fh_open;

		lock_acquire(curproc->file_table[file_desc]->fh_lock);
		curproc->file_table[file_desc]->ref_count++;
		lock_release(curproc->file_table[file_desc]->fh_lock);
	}else{
		curproc->file_table[file_desc] = filehandle_create(temp_filename, flags);
	}	
	*val = file_desc;
	return 0;
}
int
sys_close(int fd){
	if(fd < 0 || fd > 63){
		return EBADF;
	}
	struct filehandle *file_handle = NULL;
	file_handle = curproc->file_table[fd];

	if(file_handle == NULL){
	//	kprintf_n("ANAND : sys_close : going to return EBADF -  %s \n", file_handle->file_name);
		return EBADF;
	}

	//kprintf_n("ANAND : sys_close : %s \n", file_handle->file_name);
	
	lock_acquire(file_handle->fh_lock);
	//kprintf_n("ANAND : sys_close : Lock acquired \n");
	if(file_handle->ref_count > 0){
		file_handle->ref_count--;
	//	kprintf_n("ANAND : sys_close : ref_count decremented to : %d \n", file_handle->ref_count);
	}

	curproc->file_table[fd] = NULL;
	lock_release(file_handle->fh_lock);

	//kprintf_n("ANAND : sys_close : lock released \n");
	
	if(file_handle->ref_count == 0){
		filehandle_destroy(file_handle);
		file_handle = NULL;
	//	kprintf_n("ANAND : sys_close : filehandle destroyed \n");
	}
	return 0;
}

int sys_dup2(int oldfd, int newfd, int *val){

        if(newfd < 0 || newfd > 63 || oldfd < 0 || oldfd > 63 || curproc->file_table[oldfd] == NULL){   //newfd , oldfd within limits
                *val = -1;
                return EBADF;
        }

        if(newfd == oldfd){
                *val = newfd;
                return 0;
        }

        if(curproc->file_table[newfd] != NULL){
                sys_close(newfd);
        }

        lock_acquire(curproc->file_table[oldfd]->fh_lock);
        curproc->file_table[newfd] = curproc->file_table[oldfd];
        curproc->file_table[newfd]->ref_count++;
        lock_release(curproc->file_table[oldfd]->fh_lock);
        //kprintf_n("Filename at %d is %s\n",newfd,curproc->file_table[newfd]->file_name);
        *val = newfd;
        return 0;

}

int
sys_chdir(const char *pathname){
	char path_name[1024];
        size_t pathname_len = 0;
        int result = copyinstr((const_userptr_t)pathname, path_name, 1024, &pathname_len);
        if (result) {
                //*val = -1;
                return result;
        } 
	char *temp_pathname = &path_name[0];
	if(temp_pathname == NULL){
                return EFAULT;
        }

        if((temp_pathname == (void *)0x40000000) || (temp_pathname == (void *)0x80000000)){
                return EFAULT;
        }
        
	if(strlen(temp_pathname) == 0){
                return EINVAL;
        }
	result = vfs_chdir(temp_pathname);
        if (result) {
		return result;
	}
	return 0;
}

int
sys___getcwd(char *buf, size_t buflen, int *val){
	
/*	char path_name[1024];
        size_t pathname_len = 0;
        int result = copyinstr((const_userptr_t)pathname, path_name, 1024, &pathname_len);
        if (result) {
                *val = -1;
                return result;
        }
*/
	/*if(buf == NULL){
		*val = -1;
		return EFAULT;
	}
	if(strlen(buf) > buflen){
		*val = -1;
		return EFAULT;
	}*/
	
	struct iovec cwd_iov;
        struct uio cwd_uio;
	cwd_iov.iov_ubase = (userptr_t)buf;
        cwd_iov.iov_len = buflen;

        cwd_uio.uio_iov = &cwd_iov;
        cwd_uio.uio_iovcnt = 1;
        cwd_uio.uio_resid = buflen;
        cwd_uio.uio_offset = 0;
        cwd_uio.uio_segflg = UIO_USERSPACE;
        cwd_uio.uio_rw = UIO_READ;
        cwd_uio.uio_space = curproc->p_addrspace;

	int result = vfs_getcwd(&cwd_uio);
	if (result) {
                return result;
        }
	kprintf_n("ANAND - GETCWD - %s \n", buf);
	*val = buflen - cwd_uio.uio_resid;
	return 0;
}

int
sys_fork(struct trapframe *tf, int *val){
	struct proc *c_proc = NULL;
	struct trapframe *c_tf = NULL;
	int c_pid = -1;
	//vaddr_t stack_ptr;
	//kprintf_n("ANAND FORK : inside fork \n");	

	c_proc = proc_create_runprogram("child");
        if(c_proc == NULL){
                *val = -1;
                return ENOMEM;
       	}

	lock_acquire(proc_table_lock);
	for(int i = 2; i < 128; i++){
		if(proc_table[i] == NULL){
			c_pid = i;
	//		kprintf_n("ANAND FORK : Found the right pid ; %d  \n", c_pid);
			break;
		}
	}
	//proc_table[c_pid] = c_proc;

	//c_proc->pid = c_pid;
        //c_proc->ppid = curproc->pid;
	
	//lock_release(proc_table_lock);
	if(c_pid == -1){
		proc_destroy(c_proc);
		lock_release(proc_table_lock);
		*val = -1;
		return ENOMEM;
	}

	proc_table[c_pid] = c_proc;
        c_proc->pid = c_pid;
        c_proc->ppid = curproc->pid;

	lock_release(proc_table_lock);	

	c_tf = kmalloc(sizeof(struct trapframe));
	*c_tf = *tf;
	
	int mem_result = as_copy(curproc->p_addrspace, &c_proc->p_addrspace);
	if(mem_result != 0 || c_proc->p_addrspace == NULL){
		*val = -1;
		return ENOMEM;
	}
	//kprintf_n("ANAND FORK : child address space copied from parent \n");
	
	//proc_table[c_pid] = c_proc;
	
	for(int j = 0; j < 64; j++){
		if(curproc->file_table[j] != NULL){
			//c_proc->file_table[j] = kmalloc(sizeof(struct filehandle));
			c_proc->file_table[j] = curproc->file_table[j];
			//lock_acquire(c_proc->file_table[j]->fh_lock);
			c_proc->file_table[j]->ref_count++;
			//lock_release(c_proc->file_table[j]->fh_lock);	
		}
	}
	
	/*int result = as_define_stack(c_proc->p_addrspace, &stack_ptr);
        if (result) {
                return result;
        }*/
	//c_tf->tf_sp = stack_ptr;
	//kprintf_n("ANAND FORK : file handle pointers from parent are got : stack pointer is %llu  \n", (off_t)stack_ptr);
	int check = thread_fork("child",c_proc, enter_forked_process, c_tf, 0);
	if(check){
	 	*val = -1;
                return ENOMEM;
        }


	//kprintf_n("ANAND FORK : thread fork is done  \n");
	
	*val = c_proc->pid;
	return 0;
}

int
sys_execv(const char *progname, char **args, int *val){

//	(void) args;	
	
	if(progname == NULL){
                *val = -1;
                return EFAULT;
        }
        if((progname == (void *)0x40000000) || (progname == (void *)0x80000000)){
                *val = -1;
                return EFAULT;
        }
        if(args == NULL){
                *val = -1;
                return EFAULT;
        }

        if((args == (void *)0x40000000) || (args == (void *)0x80000000)){
                *val = -1;
                return EFAULT;
        }

	struct addrspace *as = NULL;
        struct vnode *vn = NULL;
	//int MAX_SIZE = 4000;
        vaddr_t entrypoint, stackptr;
        //userptr_t argv; // = (userptr_t)args;
	int result;
	//char arg_buf[MAX_SIZE]; //= kmalloc(2048*sizeof(char));
	// test the args and progname
	//kprintf_n("ANAND EXECV : the program name is : %s \n",  (char *)progname);
	//kprintf_n("ANAND EXECV : args value is %p\n", (userptr_t)args);
	
	int argc = 0;
		
	/*int args_len;
	for(args_len = 0; args[args_len] != NULL; args_len++){
		kprintf_n("ANAND EXECV :  original args addresses are : %s \n", args[args_len]);
	}
	kprintf_n("ANAND EXECV - the size of the args array is : %d \n", args_len);
	*/
	int out = 0;
	//int arg_len_arr[args_len];
	//char **arg_ptrs = kmalloc((args_len+1)*sizeof(*arg_ptrs));//[args_len+1];
	
	char **arg_ptrs = kmalloc(sizeof(char **));
	do{
		result = copyin((const_userptr_t)&args[argc], &arg_ptrs, sizeof(int)); 
		if(result){
                        *val = -1;
                        return result;
                }
		if(arg_ptrs == NULL) break;
	//do{
		//int null_count = 4-((strlen(args[argc])+1)%4);
		//int new_length = strlen(args[argc]) + 1 + null_count;
		size_t copied_length;
	//	kprintf_n("The modifed length is : %d \n ", copied_length);
		result = copyinstr((const_userptr_t)args[argc], &arg_buf[out], MAX_SIZE-out, &copied_length);
		if(result){
			*val = -1;
			return result;
		}
		//copyinstr((const_userptr_t)*(&args[argc]), &arg_buf[out], MAX_SIZE-out, &copied_length);
		int null_count = 4-((copied_length-1)%4);
		int new_length = copied_length + null_count - 1;
		/*int i;
		for(i = 0; i < new_length; i++){
			if(i < (int)strlen(args[argc])){
				arg_buf[out] = temp[i];
			}else{
				arg_buf[out] = '\0';
			}
			out++;
		}*/
		out+=new_length;
		//arg_len_arr[argc] = new_length;
	//	kprintf_n("ANAND EXEC : out is %d and copied length is %d \n ", out, copied_length);
		argc++;
	}while(1);
	//while(arg_ptrs[argc] != NULL);
	kfree(arg_ptrs);
	//arg_ptrs[args_len] = NULL;

/*	int j = 0;
	for(; j <= args_len; j++){
		kprintf_n("ANAND EXEC : args pointers values : %s and address is %p \n ", arg_ptrs[j], (void*)arg_ptrs[j]);
		unsigned int k;
		kprintf("ANAND EXECV - the new length of args are  : %d \n", strlen(arg_ptrs[j]));
		for(k = 0; k < strlen(arg_ptrs[j]); k++){
			kprintf("ANAND EXECV - buff characters are  : %c \n", arg_ptrs[j][k]);
		}
	}
*/	
/*	int j = 0;
	while(j < out){
	//	kprintf("ANAND EXECV - buff characters are  : %c \n", arg_buf[j]);
		if(arg_buf[j] == '\0'){
	//		kprintf("ANAND EXEC - hey found \n");
		}
		j++;
	} 
*/
	

	//kprintf("ANAND EXECV - after copyin - the args are : %s \n", arg_buf);
	//kfree(arg_buf);
        /* Open the file. */
	char prog_name[1024];
	size_t prog_len = 0;
	result = copyinstr((const_userptr_t)progname, prog_name, 1024, &prog_len);
        if (result) {
                *val = -1;
                return result;
        }
	if(prog_len == 1){
		*val = -1;
		return EINVAL;
	}
	result = vfs_open((char *)prog_name, O_RDONLY, 0, &vn);
        if (result) {
		*val = -1;
                return result;
        }

	//as_destroy(curproc->p_addrspace);

        /* Create a new address space. */
        as = as_create();
        if (as == NULL) {
                *val = -1;
		vfs_close(vn);
            	return ENOMEM;
        }

        /* Switch to it and activate it. */
	struct addrspace *old_as = NULL;
	old_as = proc_setas(as);
        if(old_as != NULL){
                as_destroy(old_as);
        }

        //proc_setas(as);
        as_activate();

        /* Load the executable. */
        result = load_elf(vn, &entrypoint);
        if (result) {
                /* p_addrspace will go away when curproc is destroyed */
                vfs_close(vn);
		*val = -1;
                return result;
        }

        /* Done with the file now. */
        vfs_close(vn);
	/* Define the user stack in the address space */
        result = as_define_stack(as, &stackptr);
        if (result) {
                /* p_addrspace will go away when curproc is destroyed */
                *val = -1;
		return result;
        }
	//kprintf("ANAND EXECV : stack ptr address is %p \n", (void *)stackptr);

	// copyout
	//int total_vals = out;
	//char **us_args;
	vaddr_t start_of_vals = stackptr - out;
	vaddr_t start_of_arg_ptr = start_of_vals - ((argc+1)*4);
	userptr_t argv = (userptr_t)start_of_arg_ptr;
	result = copyout((const void *)&arg_buf[0], (userptr_t)start_of_vals, (size_t)out);
	if(result){
                *val = -1;
                return result;
        }
	//kprintf("ANAND EXECV : stack ptr address after values copied is %p \n", (void *)start_of_vals);
	//kprintf("ANAND EXECV : stack ptr address - start of arg pointer is %p \n", argv);

/*	int g;
        //char *c = kmalloc(100*sizeof(char));
        char d[100];
  //      kprintf("THE COPIED IN VALUES ARE : ");
   //     copyin((const_userptr_t)start_of_vals, (void *)&d, 100);
        for(g= 0;g < 100; g++){
                //char c;
                //copyin((const_userptr_t)test_ptr, &c[f], 1);
                //kprintf("--------->%c\n", d[g]);
                //test_ptr=test_ptr-1;
        }

*/	
	int i;
	for(i = 0; i < argc; i++){
		//us_args[i] = (char *)start_of_vals;
		result = copyout(&start_of_vals, (userptr_t)start_of_arg_ptr, 4);
		if(result){
                	*val = -1;
                	return result;
        	}
	//	kprintf("ANAND EXEC - imp - values copied at %p \n ", (void *)start_of_vals);
	//	kprintf("ANAND EXEC - imp - args copied at %p \n ", (void *)start_of_arg_ptr);
		start_of_arg_ptr = start_of_arg_ptr + 4;
		 //kprintf("ANAND EXEC - the new _length is %d \n", arg_len_arr[i]);
	//	kprintf_n("ANAND EXEC - the values are %c \n", ((char *)start_of_vals)[0]);
		//start_of_vals += 4;
		while((void *)start_of_vals < (void *)0x80000000){
			start_of_vals += 4;
			char c = ((char *)start_of_vals)[0];
			if(c ==	 '\0'){
				start_of_vals += 4;
				break;
			}else if(c != '\0'){
				if(((char *)start_of_vals-1)[0] == '\0'){
					break;
				}
			}
		}
		//start_of_vals = start_of_vals + arg_len_arr[i];
	}
	
/*	userptr_t test_ptr = argv;
	int f;
	//char *c = kmalloc(100*sizeof(char));
	char c[100];
//	kprintf("THE COPIED IN VALUES ARE : ");
	copyin((const_userptr_t)test_ptr, (void *)&c, 100);	
	for(f= 0;f < 100; f++){
		//char c;
		//copyin((const_userptr_t)test_ptr, &c[f], 1);
		//kprintf("--------->%c\n", c[f]);
		//test_ptr=test_ptr-1;
	}
*/
	//kprintf_n("copy in loop ended \n");
	/*char *temp_args;
	// copy out
	int len;
	for(len=0; len < argc; len++){
		int shift = (arg_len_arr[len])/4;
		stackptr -= shift;
		copyout(arg_ptrs[len], (userptr_t)stackptr, arg_len_arr[len]);
		temp_args[len] = stackptr;
	}
	stackptr--;
	int len_a;
	for(len_a = argc-1; len_a >= 0; len_a--){
		//copyout((const void*)temp_args[len_a], (userptr_t)stackptr, sizeof(temp_args[len_a]));
		stackptr--;
	}*/
	
//	argv = stackptr;
	kfree(arg_ptrs);
	/* Warp to user mode. */
        enter_new_process(argc /*argc*/, argv /*userspace addr of argv*/,
                          NULL /*userspace addr of environment*/,
                          (vaddr_t)argv, entrypoint);

        /* enter_new_process does not return. */
        panic("enter_new_process returned\n");
        return EINVAL;
}

int
sys_getpid(int *val){
	*val = curproc->pid;
	return 0;
}

int
sys__exit(int exitcode){
	curproc->exit_status = 1;
	if(exitcode == 0){
		curproc->exit_code = _MKWAIT_EXIT(exitcode);	
	}
	else{
                curproc->exit_code = _MKWAIT_SIG(exitcode);
        }
	/*if(!spinlock_do_i_hold(&curproc->p_lock))
	{
		spinlock_acquire(&curproc->p_lock);
	}
	curproc->exit_status = 1;
	if(spinlock_do_i_hold(&curproc->p_lock))
	{
	spinlock_release(&curproc->p_lock);
	}*/
	//curproc->exit_status = 1;	
	//V(curproc->p_semaphore);

	thread_exit();
	//kprintf_n("ANAND - exit - process successfully EXITED \n");
	return 0;
}

int
sys_waitpid(int pid, int *status, int opt, int *val){
	if(opt != 0){
		*val = -1;
		return EINVAL;
	}	
	if((pid < 1 || pid > 127) || (proc_table[pid] == NULL)){
		*val = -1;
		return ESRCH;
	}
	if(proc_table[pid]->ppid != curproc->pid){
		*val = -1;
		return ECHILD;
	}
	
	if (pid == curproc->ppid) {
		*val = -1;
		return ECHILD;
	}

	if((status == (void *)0x40000000) || (status == (void *)0x80000000)){
        	*val = -1;
                return EFAULT;
       	}
	
	/*if ((vaddr_t)status >= (vaddr_t)USERSPACETOP) {// region is within the kernel 
        	*val = -1;
        	return EFAULT;
   	}*/

	// if doesnot work, try adding lock.
	

	struct proc *child = proc_table[pid];
	

	//int c_status = -1; 
	//while(proc_table[pid]->exit_status != 1);
	/*while(c_status != 1){
		 if(!spinlock_do_i_hold(&curproc->p_lock))
	        {
        	        spinlock_acquire(&curproc->p_lock);
        	}
		c_status = child->exit_status;
        	if(spinlock_do_i_hold(&curproc->p_lock))
        	{
        		spinlock_release(&curproc->p_lock);
        	}

	}*/

	//if(child->exit_status != 1){	
		P(child->p_semaphore);
	//}
	*val = proc_table[pid]->pid;
	//-------int temp_status = _MKWAIT_EXIT(proc_table[pid]->exit_code);
	if(status != NULL){
		//------*status = proc_table[pid]->exit_code;
		int check = copyout((const void *)&proc_table[pid]->exit_code,(userptr_t)status,sizeof(int));
		//copyout((const void *)&proc_table[pid]->exit_code,(userptr_t)status,sizeof(int));               	
//------int check = copyout((const void *)&temp_status, (userptr_t)status, sizeof(int)); 
		if(check){
			*val = -1;
                        return check;
               	}
	}
	proc_destroy(proc_table[pid]);
	proc_table[pid] = NULL;
	//kprintf_n("ANAND - waitpid - process successfully destroyed \n");
	return 0;
}

int
sys_sbrk(intptr_t amount, int *val){
	
	/*(void) amount;
	(void) val;
	return 0;*/

	struct addrspace *as = NULL;
	as = curproc->p_addrspace;
	vaddr_t cur_end_addr = as->as_heap_end_pointer;
	if(amount == 0){
		*val = as->as_heap_end_pointer;
		return 0;
	}
	if(amount > 0){
		if((as->as_heap_end_pointer + amount) >= (as->as_stackpointer - STACK_SIZE*PAGE_SIZE)){
			amount = 0;
			*val = -1;
			return ENOMEM;
		}else{
			as->as_heap_end_pointer += amount;
			if((as->as_heap_end_pointer & ~(vaddr_t)PAGE_FRAME) > 0){
				amount = 0;
				*val = -1;
				return EINVAL;
			}else if((as->as_heap_end_pointer & ~(vaddr_t)PAGE_FRAME) == 0){
				amount = 0;
				*val = cur_end_addr;
				cur_end_addr = 0;
				return 0;
			}
		}	
	}else{
		if(((long)as->as_heap_end_pointer + amount) >= (long)as->as_heap_start_pointer){
			// with in range of heap 
			as->as_heap_end_pointer += amount;
                        if((as->as_heap_end_pointer & ~(vaddr_t)PAGE_FRAME) > 0){
				amount = 0;
                                *val = -1;
                                return EINVAL;
                        }else if((as->as_heap_end_pointer & ~(vaddr_t)PAGE_FRAME) == 0){
				for(vaddr_t st = as->as_heap_end_pointer; st <= cur_end_addr; st+=PAGE_SIZE){
					struct page_table_entry *pte;
					vaddr_t temp_va = (st & (vaddr_t)PAGE_FRAME);// >> 12;
					pte = as->as_pte_start;
					if(pte != NULL && pte->v_pn == temp_va){
						as->as_pte_start = pte->next_pte;
						free_ppages(pte);
                                                int spl = splhigh();
                                                int index = tlb_probe(pte->v_pn, 0);
                                                if(index > 0){
                                                       tlb_write(TLBHI_INVALID(index), TLBLO_INVALID(), index);
                                                }
                                                splx(spl);
                                                kfree(pte);
						pte = NULL;
					}else{
					while(pte->next_pte != NULL){
						//vaddr_t temp_va = (st & (vaddr_t)PAGE_FRAME) >> 12;
						if(pte->next_pte->v_pn == temp_va){
							struct page_table_entry *temp_pte = pte->next_pte;
							struct page_table_entry *temp2_pte = pte->next_pte->next_pte;
							free_ppages(temp_pte);
							int spl = splhigh();
							int index = tlb_probe(temp_pte->v_pn, 0);
							if(index > 0){
								tlb_write(TLBHI_INVALID(index), TLBLO_INVALID(), index); 
							}	
							splx(spl);
							kfree(temp_pte);
							temp_pte = NULL;
							pte->next_pte = temp2_pte;
							break;
						}else{
							pte = pte->next_pte;
						}
					}
					}
				}
                        }
		}else{
			amount = 0;
			*val = -1;
			return EINVAL;
		}
	}
	amount = 0;
	*val = cur_end_addr;
	cur_end_addr = 0;
	return 0;
}
