/* Liam GLockner and Calista Greenway (group 8)
   Assignment 1 (part 2)
   Due: 3/24/2022
*/


#include <cstdlib> //standard library for c++
#include <iostream> //input output stream library
#include <deque> // double ended queue
#include <map>   //map library 
#include "interrupt.h" // the included interrupt library 
#include "thread.h"    // the included header file 
#include <iterator>    // iterators 
#include <ucontext.h>  // library that includes get and set, make and swap for context switching 


using namespace std;

struct Thread { 
//defines the thread structure, composed of id, stack identifier, context pointer for context switching and flag variable for completed or not
	unsigned int id;
	char* stack;
	ucontext_t* context_pointer;
	bool completed;	
	
};

struct Lock {
// defines the lock structure, composed of a pointer to the thread that owns the lock and the double ended queue composed of blocked threads. 
	Thread* owner;
	deque<Thread*>* blocked_threads;
};


typedef map<unsigned int, deque<Thread*>*>::const_iterator cv_iterator;
typedef map<unsigned int, Lock*>::const_iterator lock_iterator;
typedef void(*thread_startfunc_t)(void*);


//=============== variable declarations begin here =====================================

static Thread* currThread; // the pointer to the current thread 
static ucontext_t* scheduler; 
static deque<Thread*> ready; // the queue of threads that are ready 

static map<unsigned int, Lock*> locks_map;
static map<unsigned int, deque<Thread*>*> cv_map;


static bool initDone = false; //flag that triggers when thread_libinit runs 
static int id = 0;

//============== helper functions begin here ===========================================
void delete_thread() { //function to delete the current thread and clear the context variables
	delete currThread->stack; //clear the stack
	currThread->context_pointer->uc_stack.ss_sp = NULL; //clear stack pointer
	currThread->context_pointer->uc_stack.ss_size   = 0;  //clear stack sie pointer
	currThread->context_pointer->uc_stack.ss_flags = 0;  //clear flags variable
	currThread->context_pointer->uc_link = NULL;  //clear link var
	delete currThread->context_pointer; //delete the context pointer
	delete currThread; //delete the thread
	currThread = NULL; // set the thread to NULL
}

static int execute_function (thread_startfunc_t func, void *arg) { 
  interrupt_enable();
  func(arg); // run the function
  interrupt_disable();

  currThread->completed = true; //set the status flag to complete 
  swapcontext(currThread->context_pointer, scheduler); // swap to the next sceduled task
  return 0;
}

static int unlock_NoInterrupts(unsigned int lock) {
	lock_iterator litr = locks_map.find(lock);
	Lock* lock1;
	if(litr == locks_map.end()) {
		return -1;
	}
	else {
		lock1 = (*litr).second;
		if(lock1->owner == NULL)
			return -1;
		else {
			if (lock1->owner->id == currThread->id) {
				if(lock1->blocked_threads->size() > 0) {
					lock1->owner = lock1->blocked_threads->front();
					lock1->blocked_threads->pop_front();
					ready.push_back(lock1->owner);  
				}
				else {
					lock1->owner = NULL;
				}
			}
			else {
				return -1;
			} 		
		}	
	}
	return 0;
}
//============== Primary Functions begin here ==================================================

int thread_libinit(thread_startfunc_t func, void *arg){

	if(initDone == true) // if the intit function has already run return -1 for failure
		return -1;
	
	initDone = true; // if not set the flag to true 	
	
	if(thread_create(func,arg) < 0) {
		return -1;
	} 
	// if thread create fails return -1 for failure 
		
	Thread* first_thread = ready.front(); //push first thread to the front of the queue 
	ready.pop_front();			      
	currThread = first_thread;         // set the current thread pointer to the first thread
	
	try {
		scheduler = new ucontext_t;
	}
	catch (std::bad_alloc bad) { //if a bad allocation to memory 
		delete scheduler; //delete the placeholder context for scheduler
		return -1;	 // return -1 for failure
	}

	getcontext(scheduler); 

	//switch to the new currThread
	interrupt_disable();
	swapcontext(scheduler, first_thread->context_pointer);
	
	while(ready.size() > 0) { //if there is a ready thread 
		if (currThread->completed == true) {  //if current thread is done delete it
			delete_thread();
		}
		
		Thread* next_thread = ready.front(); //move the next threda to the top of the ready list 
		ready.pop_front(); //pop it off the queue
		currThread = next_thread; //set the currThread pointer to the next thread
		swapcontext(scheduler, currThread->context_pointer); //replace the scheduler place holder with the current thread
	}
	
	if(currThread != NULL) {
		delete_thread();	
	}
	
// if no more threads/all threads finished running :)
	cout << "Thread library exiting.\n";
	exit(0);
}

int thread_create(thread_startfunc_t func, void *arg){
	if(initDone == false) { //if the init function has not run exit and return -1 for failure
		return -1;	
	}
	
	interrupt_disable();
	Thread* newthread;
	
	try {
		newthread = new Thread;
		newthread->context_pointer = new ucontext_t;
		getcontext(newthread->context_pointer);

		newthread->stack = new char [STACK_SIZE];
		newthread->context_pointer->uc_stack.ss_sp = newthread->stack;
		newthread->context_pointer->uc_stack.ss_size = STACK_SIZE;
		newthread->context_pointer->uc_stack.ss_flags = 0;
		newthread->context_pointer->uc_link = NULL;
		makecontext(newthread->context_pointer,(void (*)())execute_function, 2, func,arg);

		newthread->id = id;
		id++;
		newthread->completed = false;
		ready.push_back(newthread);
	}
	catch (std::bad_alloc bad) {
		delete newthread->context_pointer;
		delete newthread->stack;
		delete newthread;
		interrupt_enable();
		return -1;	
	}

	interrupt_enable();
	return 0;
}

int thread_yield(void){
	if (initDone == false) { //if the init function has not run exit and return -1 for failure
		return -1;	
	}

	interrupt_disable();
	ready.push_back(currThread);
	swapcontext(currThread->context_pointer,scheduler);
	interrupt_enable();
	return 0;

}

int thread_lock(unsigned int lock){ 
	if (initDone == false) {
		return -1;	
	}

	interrupt_disable();

	lock_iterator litr = locks_map.find(lock);
	Lock* lock2;
	if(locks_map.find(lock) == (locks_map.end())) {
		try {
			lock2 = new Lock;
			lock2->owner = currThread;
			lock2 ->blocked_threads = new deque<Thread*>;

		}
		catch (std::bad_alloc bad){
			delete lock2->blocked_threads;
			delete lock2;
			interrupt_enable();
			return -1;
		}
		locks_map.insert(make_pair(lock,lock2));
	}
	else {
		lock2 = (*litr).second;
		if(lock2->owner == NULL) {
			lock2->owner = currThread;
		}
		else {
			if(lock2->owner->id == currThread->id) {
				interrupt_enable();
				return -1;
			}
		
			else {
				lock2->blocked_threads->push_back(currThread);
				swapcontext(currThread->context_pointer,scheduler);
			}
		}
	}
	interrupt_enable();
	return 0;
}

int thread_unlock(unsigned int lock){
	if(initDone == false) {
		return -1;	
	}
	interrupt_disable();
	int result = unlock_NoInterrupts(lock);
	interrupt_enable();
	return result;

	
}

int thread_wait(unsigned int lock, unsigned int cond){
	if(initDone == false) {
		return -1;
	}
	interrupt_disable();

	if (unlock_NoInterrupts(lock) == 0) {
		cv_iterator citr = cv_map.find(cond);
		if(citr == cv_map.end()) {
			deque<Thread*>* waiting_threads;
		
			try {
				waiting_threads = new deque<Thread*>;
			}
			catch (std::bad_alloc bad) {
				delete waiting_threads;
				interrupt_enable();
				return -1;
			}
		waiting_threads->push_back(currThread);
		cv_map.insert(make_pair(cond,waiting_threads));
		}
	
	else {
		(*citr).second->push_back(currThread);
	}
	swapcontext(currThread->context_pointer,scheduler);
	interrupt_enable();
	return thread_lock(lock);
	}
	
	interrupt_enable();
	return -1;
}

int thread_signal(unsigned int lock, unsigned int cond){
	if(initDone == false) {
		return -1;
	}
	
	interrupt_disable();

	cv_iterator citr = cv_map.find(cond); //find the condition variable in the map 
	if(citr != cv_map.end()) { // if the iterator finds the condition variable
		deque<Thread*>* cv = (*citr).second; // move the condition variable 

		if(!cv->empty()) { // if thread waiting 
			Thread* thr = cv->front(); //make a thread and set it equal to the front of the cv queue 
			cv->pop_front(); // pop the first thread off the cv queue 
			ready.push_back(thr); // push back the thread to the ready queue. 
		}
		// if thread waiting for cond var, pop it off waiting queue and push to front of ready queue. 
	}
	
	interrupt_enable();
	return 0;

}

int thread_broadcast(unsigned int lock, unsigned int cond){ 
	if(initDone == false) {
		return -1;
	}
	
	interrupt_disable();
	cv_iterator citr = cv_map.find(cond);
	if(citr != cv_map.end()) {
		deque<Thread*>* cv = (*citr).second;

		while(!cv->empty()) {
			Thread* thr = cv->front();
			cv->pop_front();
			ready.push_back(thr);
		}
	}
	
	interrupt_enable();
	return 0;
}
