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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

volatile int male_num;
volatile int female_num;
volatile int match_maker_num;
//struct spinlock wm_spinlock;
struct lock *male_lock;
struct lock *female_lock;
struct lock *matchmaker_lock;

/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
	male_num=0;
	female_num=0;
	match_maker_num=0;
	//spinlock_init(&wm_spinlock);
	male_lock = lock_create("male");
	female_lock = lock_create("female");
	matchmaker_lock = lock_create("matchmaker");
	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	KASSERT(male_num == 0 && female_num == 0 && match_maker_num == 0);
	//spinlock_cleanup(&wm_spinlock);
	lock_destroy(male_lock);
	lock_destroy(female_lock);
	lock_destroy(matchmaker_lock);
	return;
}

void
male(uint32_t index)
{
	//(void)index;
	/*
	 * Implement this function by calling male_start and male_end when
	 * appropriate.
	 */

	male_start(index);
	//spinlock_acquire(&wm_spinlock);
	lock_acquire(male_lock);
	female_num--;	//match_maker_num--;
	while(1){
		//if(female_num == 0 && match_maker_num == 0){
		if(female_num == 0){
			break;
		}
	}
	//female_num--;
	//spinlock_release(&wm_spinlock);
	//male_end(index);
	lock_release(male_lock);
	male_end(index);
	return;
}

void
female(uint32_t index)
{
	//(void)index;
	/*
	 * Implement this function by calling female_start and female_end when
	 * appropriate.
	 */
	female_start(index);
    //spinlock_acquire(&wm_spinlock);
	lock_acquire(female_lock);
    	male_num--;	//match_maker_num--;
        while(1){
                //if(male_num == 0 && match_maker_num == 0){
		if(male_num == 0){
                        break;
                }
        }
	//match_maker_num--;
    //spinlock_release(&wm_spinlock);    
	//female_end(index);
	lock_release(female_lock);
	female_end(index);

	return;
}

void
matchmaker(uint32_t index)
{
//	(void)index;
	/*
	 * Implement this function by calling matchmaker_start and matchmaker_end
	 * when appropriate.
	 */
	
	matchmaker_start(index);
    //spinlock_acquire(&wm_spinlock);
    lock_acquire(matchmaker_lock);
	//match_maker_num++;
	//female_num++;
    while(1){
            if(male_num != 0 && female_num != 0){
                    break;
            }
    }
	male_num++;	female_num++; //match_maker_num+=2;
    //match_maker_num--;
    //spinlock_release(&wm_spinlock);
       
	//matchmaker_end(index);
	lock_release(matchmaker_lock);
	matchmaker_end(index);

	return;
}
