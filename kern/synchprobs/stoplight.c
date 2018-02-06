/*
 * Copyright (c) 2001, 2002, 2009
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

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>


struct lock *quad_locks[4];
//struct semaphore *quad_sem[4];
struct spinlock sl_spinlock;

/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {
	quad_locks[0] = lock_create("0quad_lock");
	quad_locks[1] = lock_create("1quad_lock");
	quad_locks[2] = lock_create("2quad_lock");
	quad_locks[3] = lock_create("3quad_lock");
	spinlock_init(&sl_spinlock);
	return;
}

/*stoplight_init() {
	quad_sem[0] = sem_create("0quad_sem",1);
	quad_sem[1] = sem_create("1quad_sem",1);
	quad_sem[2] = sem_create("2quad_sem",1);
	quad_sem[3] = sem_create("3quad_sem",1);
	spinlock_init(&sl_spinlock);
	return;
}*/

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(quad_locks[0]);
	lock_destroy(quad_locks[1]);
	lock_destroy(quad_locks[2]);
	lock_destroy(quad_locks[3]);
	spinlock_cleanup(&sl_spinlock);
	return;
}

/*void stoplight_cleanup() {
	sem_destroy(quad_sem[0]);
	sem_destroy(quad_sem[1]);
	sem_destroy(quad_sem[2]);
	sem_destroy(quad_sem[3]);
	spinlock_cleanup(&sl_spinlock);
	return;
}*/

void
turnright(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	/*
	 * Implement this function.
	 */
	lock_acquire(quad_locks[direction]);
    inQuadrant(direction, index);
    leaveIntersection(index);
    lock_release(quad_locks[direction]);

	return;
}

/*void
turnright(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	P(quad_sem[direction]);
    	inQuadrant(direction, index);
    	leaveIntersection(index);
    	V(quad_sem[direction]);

	return;
}*/

void
gostraight(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	/*
	 * Implement this function.
	 */
	if(direction == 0){	
		lock_acquire(quad_locks[direction]);
		lock_acquire(quad_locks[(direction+3)%4]);
	}
	else{
        	lock_acquire(quad_locks[(direction+3)%4]);
		lock_acquire(quad_locks[direction]);
	}
	inQuadrant(direction, index);
	inQuadrant((direction+3)%4, index);
	lock_release(quad_locks[direction]);
	leaveIntersection(index);
	//lock_release(quad_locks[direction]);
	lock_release(quad_locks[(direction+3)%4]);

	return;
}

/*void
gostraight(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	
	// Implement this function.
	
	if(direction == 0){	
		P(quad_sem[direction]);
		P(quad_sem[(direction+3)%4]);
	}
	else{
        	P(quad_sem[(direction+3)%4]);
		P(quad_sem[direction]);
	}
	inQuadrant(direction, index);
	inQuadrant((direction+3)%4, index);
	V(quad_sem[direction]);
	leaveIntersection(index);
	//V(quad_sem[direction]);
	V(quad_sem[(direction+3)%4]);

	return;
}*/

void
turnleft(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	/*
	 * Implement this function.
	 */
	//spinlock_acquire(&sl_spinlock);
	if(direction == 0){
		lock_acquire(quad_locks[direction]);
        lock_acquire(quad_locks[(direction+2)%4]);
		lock_acquire(quad_locks[(direction+3)%4]);
	}else if(direction == 1){
		lock_acquire(quad_locks[(direction+3)%4]);
        lock_acquire(quad_locks[direction]);
        lock_acquire(quad_locks[(direction+2)%4]);
	}else{
		lock_acquire(quad_locks[(direction+2)%4]);
		lock_acquire(quad_locks[(direction+3)%4]);
		lock_acquire(quad_locks[direction]);
	}
    	inQuadrant(direction, index);
    	inQuadrant((direction+3)%4, index);
    	lock_release(quad_locks[direction]);
	inQuadrant((direction+2)%4, index);
    	lock_release(quad_locks[(direction+3)%4]);
	leaveIntersection(index);
	//lock_release(quad_locks[direction]);
    	//lock_release(quad_locks[(direction+3)%4]);
	lock_release(quad_locks[(direction+2)%4]);
	//spinlock_release(&sl_spinlock);
	return;
}

/*void
turnleft(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	
	// Implement this function.
	
	//spinlock_acquire(&sl_spinlock);
	if(direction == 0){
		P(quad_sem[direction]);
        	P(quad_sem[(direction+2)%4]);
		P(quad_sem[(direction+3)%4]);
	}
	else if(direction == 1){
		P(quad_sem[(direction+3)%4]);
        	P(quad_sem[direction]);
	        P(quad_sem[(direction+2)%4]);
	}
	else{
		P(quad_sem[(direction+2)%4]);
		P(quad_sem[(direction+3)%4]);
		P(quad_sem[direction]);
	}
    	inQuadrant(direction, index);
    	inQuadrant((direction+3)%4, index);
    	V(quad_sem[direction]);
	inQuadrant((direction+2)%4, index);
    	V(quad_sem[(direction+3)%4]);
	leaveIntersection(index);
	//V(quad_sem[direction]);
    	//V(quad_sem[(direction+3)%4]);
	V(quad_sem[(direction+2)%4]);
	//spinlock_release(&sl_spinlock);
	return;
}*/
