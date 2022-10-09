//Liam Glockner and Calista Greenway (group 8)
//ECE 570 
//Assignment 2
//3/29/2022


#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stack>
#include <map>
#include <queue>
#include <iterator>
#include <assert.h>
#include "vm_pager.h"


using namespace std;

//=========================== Structs, Typedefs, Vars ================================

struct page {
    page_table_entry_t* pte_pointer;
    unsigned int disk_block;
    bool written; 
    bool dirty;
    bool valid; 
    bool res;
    bool ref;
   
};

struct process {
    page_table_t* pgtbl_pointer;
    page** pages;
    int top_index;
};


stack<unsigned int> free_pgs;
stack<unsigned int> free_blocks;
unsigned int num_pgs;
unsigned int num_blocks;

pid_t curr_id;
process* curr_process;
map<pid_t,process*> proc_map;
queue<page*> queueforclock;

typedef map<pid_t, process*>::const_iterator proc_iter;

//======================== Helper Functions begin here ============================

void rmvpg (page* p) {
    page* pgtormv = queueforclock.front(); // set the pointer for page to be removed to front of queue
    while (pgtormv != p) { //find the page 
        queueforclock.pop();
        queueforclock.push(pgtormv);
        pgtormv=queueforclock.front();
    }
    queueforclock.pop(); // pop the page off the queue 
    pgtormv = NULL; // set to NULL
}

void clearpg() { // function to clear a used page and re-add it to the free_pgs stack
    page* temppg = queueforclock.front(); 
    assert(temppg->valid); //make sure that the page is valid
    
    while(temppg->ref == true) { //while referenced
        temppg->ref = false; //set ref to false
        temppg->pte_pointer->read_enable = 0; 
        temppg->pte_pointer->write_enable = 0;

        queueforclock.pop(); //pop the top of the queue
        queueforclock.push(temppg); //push the temp page to the queue
        temppg = queueforclock.front(); //move it to front
    }

    if(temppg->dirty == true && temppg->written == true) { // if written to and dirty 
        disk_write(temppg->disk_block, temppg->pte_pointer->ppage);
    }
    temppg->pte_pointer->read_enable = 0;
    temppg->pte_pointer->write_enable = 0;
    temppg->res = false;

    free_pgs.push(temppg->pte_pointer->ppage); // push the newly freed page to the free pgs stack
    queueforclock.pop(); // pop the clock queue
}

//======================= Primary Functions begin here ============================
/*
 * vm_init
 *
 * Called when the pager starts.  It should set up any internal data structures
 * needed by the pager, e.g. physical page bookkeeping, process table, disk
 * usage table, etc.
 *
 * vm_init is passed both the number of physical memory pages and the number
 * of disk blocks in the raw disk.
 */

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {

    for(unsigned int i = 0; i < memory_pages; i++) { // for each page in memory
        free_pgs.push(i); // push to the free page list
    }

    for(unsigned int i =0; i < disk_blocks; i++) { // for each disk block in memory
        free_blocks.push(i); // push to the free blocks list 
    }

    page_table_base_register = NULL; //initialize the page table register 

    num_pgs = memory_pages; //initialize the var for number of pages
    num_blocks = disk_blocks; //initialize the var for number of blocks
}



/*
 * vm_create
 *
 * Called when a new process, with process identifier "pid", is added to the
 * system.  It should create whatever new elements are required for each of
 * your data structures.  The new process will only run when it's switched
 * to via vm_switch().
 */
void vm_create(pid_t pid){
    process* proc1 = new process; // initialize the new process pointer

    proc1->pgtbl_pointer = new page_table_t; //initialize the page table pointer to a new page table 
    proc1->pages = new page*[VM_ARENA_SIZE / VM_PAGESIZE]; //initialize pages pointer to new page array
    proc1->top_index = -1; // set the index to -1, no pte is valid at innit
    proc_map[pid] = proc1; // add the new process with the given pid to the map

}

/*
 * vm_switch
 *
 * Called when the kernel is switching to a new process, with process
 * identifier "pid".  This allows the pager to do any bookkeeping needed to
 * register the new process.
 */
void vm_switch(pid_t pid){
    proc_iter i = proc_map.find(pid); // find the pid in the process map

    if(i != proc_map.end()) {
        curr_id = pid; // set the current id to the pid
        curr_process = (*i).second; //set current process to process found with pid
        page_table_base_register = curr_process->pgtbl_pointer; //set the PTBR to the page table pointer of the current process
    }
}

/*
 * vm_fault
 *
 * Called when current process has a fault at virtual address addr.  write_flag
 * is true if the access that caused the fault is a write.
 * Should return 0 on success, -1 on failure.
 */
int vm_fault(void *addr, bool write_flag){
     
     if(((unsigned long long)addr - (unsigned long long)VM_ARENA_BASEADDR) >= (curr_process->top_index+1)*VM_PAGESIZE) {
        return -1;
    }

    page* pg = curr_process->pages[((unsigned long long)addr - (unsigned long long) VM_ARENA_BASEADDR) / VM_PAGESIZE];

    pg->ref = true;

    

    if(write_flag == true){
        if(pg->res == false) {
            if(free_pgs.empty()) {
                clearpg();
            }
            pg->pte_pointer->ppage = free_pgs.top();
            free_pgs.pop();
            
            if(pg->written==false){
                memset(((char*) pm_physmem) + pg->pte_pointer->ppage * VM_PAGESIZE, 0, VM_PAGESIZE);
                pg->written = true;
            }
            else {
                disk_read(pg->disk_block,pg->pte_pointer->ppage);
            }

            queueforclock.push(pg);
            pg->res = true;
        }
        pg->pte_pointer->read_enable = 1;
        pg->pte_pointer->write_enable = 1;
        pg->dirty = true;
    }
    
    else {
        if (pg->res==false) {
            if(free_pgs.empty()) {
                clearpg();
            }

            pg->pte_pointer->ppage = free_pgs.top();
            free_pgs.pop();

            if(pg->written==false) {
                memset(((char *) pm_physmem) + pg->pte_pointer->ppage * VM_PAGESIZE, 0, VM_PAGESIZE);
                pg->dirty = false;
            }
            else {
                disk_read(pg->disk_block, pg->pte_pointer->ppage);
                pg->dirty=false;
            }
            queueforclock.push(pg);
            pg->res = true;
        }
        if(pg->dirty == true) {
            pg->pte_pointer->write_enable = 1;
        }
        else {
            pg->pte_pointer->write_enable = 0;
        }
        pg->pte_pointer->read_enable = 1;
        pg->ref = true;

    }
      
    pg = NULL;
    return 0;
}


/*
 * vm_destroy
 *
 * Called when current process exits.  It should deallocate all resources
 * held by the current process (page table, physical pages, disk blocks, etc.)
 */
void vm_destroy(){
 for(unsigned int i =0; i <= curr_process->top_index; i++) { //for all the pages in the current process
     page* p1 = curr_process->pages[i]; //create a pointer to the page

     if(p1->res == true) { //if a resident 
         free_pgs.push(p1->pte_pointer->ppage); // free the page
         rmvpg(p1); //remove the page
     }
     free_blocks.push(p1->disk_block); //free the disk blocks used by the process
     p1->valid = false; 
     delete p1; // delete the page
     p1=NULL; //set the pointer to null for reuse
 }
    delete curr_process; //delete the current process
    proc_map.erase(curr_id); //remove the id of the current process from the map 

    curr_process = NULL; //make the current process NULL for reuse 
    page_table_base_register = NULL; //set the PTBR to NULL
}


/*
 * vm_extend
 *
 * A request by current process to declare as valid the lowest invalid virtual
 * page in the arena.  It should return the lowest-numbered byte of the new
 * valid virtual page.  E.g., if the valid part of the arena before calling
 * vm_extend is 0x60000000-0x60003fff, the return value will be 0x60004000,
 * and the resulting valid part of the arena will be 0x60000000-0x60005fff.
 * vm_extend should return NULL on error, e.g., if the disk is out of swap
 * space.
 */
void * vm_extend(){

    if((curr_process->top_index+1) >= VM_ARENA_SIZE/VM_PAGESIZE) {
        return NULL;
    }
    if (free_blocks.empty()) {
        return NULL;
    }
    curr_process->top_index++;

    page* p1 = new page;

    p1->pte_pointer = &(page_table_base_register->ptes[curr_process->top_index]);

    p1->disk_block = free_blocks.top();
    free_blocks.pop();

    p1->pte_pointer->read_enable = 0;
    p1->pte_pointer->write_enable = 0;

    p1->ref = false;
    p1->res = false;
    p1->written = false;
    p1->dirty = false;
    p1->valid = true;


    curr_process->pages[curr_process->top_index] = p1;

    return (void *) ((unsigned long long) VM_ARENA_BASEADDR + curr_process->top_index*VM_PAGESIZE);
}

/*
 * vm_syslog
 *
 * A request by current process to log a message that is stored in the process'
 * arena at address "message" and is of length "len".
 *
 * Should return 0 on success, -1 on failure.
 */
int vm_syslog(void *message, unsigned int len){
    //======== check if message is within bounds and do error checking for len ======
    if (
            ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR + len) >= (curr_process->top_index + 1) * VM_PAGESIZE ||
            ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR) >= (curr_process->top_index + 1) * VM_PAGESIZE ||
            ((unsigned long long) message < (unsigned long long) VM_ARENA_BASEADDR) ||
            len <= 0
       ) {
           return -1;
       }
        
    //===== end of error checking 
    
    string str; // output string variable

    for(unsigned int i = 0; i < len; i++) {
        unsigned int page_num = ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR + i) / VM_PAGESIZE;
        unsigned int page_offset = ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR + i) % VM_PAGESIZE;
        unsigned int pf = page_table_base_register->ptes[page_num].ppage;

        if(page_table_base_register->ptes[page_num].read_enable == 0 || curr_process->pages[page_num]->res == false) {
           if(vm_fault((void*) ((unsigned long long) message + i), false)) {
               return -1;
           } 
           pf = page_table_base_register->ptes[page_num].ppage;
        }
        curr_process->pages[page_num]->ref = true;
        str.append((char *) pm_physmem+pf * VM_PAGESIZE+page_offset,1);
    }
    cout << "syslog \t\t\t" << str << endl;
    return 0;
}
