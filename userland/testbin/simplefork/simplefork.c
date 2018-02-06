#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <test/test.h>
#include <test161/test161.h>


int main(void) {

	pid_t pid = fork();
	if (pid == -1) {
		printf("fork failed\n");
		_exit(1);
	} else if (pid == 0) {	
		printf("Hello from the child process!. my pid is %d \n", getpid());
		_exit(0);
	} else {
		int status;
		printf("Before waitpid \n");
		int c_pid = waitpid(pid, &status, 0);
		printf("The child that exited is %d \n", c_pid);
	}
	printf("Fork , waitpid and exit");
	return 0;

}


/*int
main(void){
        pid_t pid = fork();
        if(pid < 0){
                printf("the pid is negative \n");
        }else if(pid == 0){
                printf("HI CHILD HERE : PID IS : %d  \n", getpid());
                //warn("Child with pid as : %d , and ppid as : %d \n", curproc->pid, curproc->ppid);
        	pid_t pid1 = fork();
		if(pid1 == 0){
			printf("---------------- %d \n" , getpid());
			//printf("HELLO CHILD'S CHILD HERE, THE CHILD'S CHILD PID IS %d  \n", getpid());
		}else{
			printf("****************** %d \n", getpid());
			//printf("FIRST CHILD, GAVE BIRTH TO A CHILD, SO MY PID IS : %d \n", getpid());
		}
		
	}else if(pid > 0){
                printf("parent here, the parent's pid is %d  \n", getpid());
        }
        printf("Fork done ! \n");
        success(TEST161_SUCCESS, SECRET, "/testbin/simplefork");
	return 0;
}*/
