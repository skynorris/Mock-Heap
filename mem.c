////////////////////////////////////////////////////////////////////////////////
// Main File:        mem.c
// This File:        mem.c
// Other Files:
// Semester:         CS 354 Fall 2016
//
// Author:           Skyler Norris
// Email:            sgnorris@wisc.edu
// CS Login:         skyler

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include "mem.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
    /* The blocks are maintained as a linked list */
    /* The blocks are ordered in the increasing order of addresses */
    struct block_hd* next;

    /* The size_status field is the size of the payload+padding and is always a multiple of 4 */
    /* ie, last two bits are always zero - can be used to store other information*/
    /* LSB = 0 => free block */
    /* LSB = 1 => allocated/busy block */

    /* So for a free block, the value stored in size_status will be the same as the block size*/
    /* And for an allocated block, the value stored in size_status will be one more than the block size*/

    /* The value stored here does not include the space required to store the header */

    /* Example: */
    /* For a block with a payload of 24 bytes (ie, 24 bytes data + an additional 8 bytes for header) */
    /* If the block is allocated, size_status should be set to 25, not 24!, not 23! not 32! not 33!, not 31! */
    /* If the block is free, size_status should be set to 24, not 25!, not 23! not 32! not 33!, not 31! */
    int size_status;

}block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;


int isFree(block_header *block){
    if(block->size_status %4 ==0){
        return 1;
    }
    else{
        return 0;
    }
}

/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion)
{
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void* space_ptr;
    static int allocated_once = 0;

    if(0 != allocated_once)
    {
        fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
        return -1;
    }
    if(sizeOfRegion <= 0)
    {
        fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    /* Get the pagesize */
    pagesize = getpagesize();

    /* Calculate padsize as the padding required to round up sizeOfRegio to a multiple of pagesize */
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    /* Using mmap to allocate memory */
    fd = open("/dev/zero", O_RDWR);
    if(-1 == fd)
    {
        fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == space_ptr)
    {
        fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    /* To begin with, there is only one big, free block */
    list_head = (block_header*)space_ptr;
    list_head->next = NULL;
    /* Remember that the 'size' stored in block size excludes the space for the header */
    list_head->size_status = alloc_size - (int)sizeof(block_header);

    return 0;
}


/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the best free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */

void* Mem_Alloc(int size)
{
    // Check size and round it up to a multiple of 4
    if(size%4 !=0){
        size -= size%4;
        size += 4;
    }
    int request_size = size;

    // data fields
    int cushion = 10000000; //extra space in block
    int found = 0;
    int whole_block = request_size + sizeof(block_header);
    block_header *itr = list_head;
    block_header *best_fit;
    block_header *next_blockptr;

    //best fit algo, traverse linked list
    while(NULL != itr) {
        if (isFree(itr)){
            if ( request_size < (itr->size_status )) {
                if ((itr->size_status - whole_block) < cushion) { //only eneter if better than previous cushion
                    cushion = (itr->size_status - whole_block);
                    best_fit = itr;
                    found = 1;
                }
            }
        }
        //keep track of how far weve gone from list head, do not add last bit if allocated
        itr = itr->next;


    }

    // If a block is found, check to see if we can split it
    // i.e it has space leftover for a new block(header + payload)
    if(found){

        //assign block to be allocated
        best_fit->size_status = request_size;


        //if can be split
        if(cushion >= (sizeof(block_header) + 4)){

            //set next block to after payload of allocate block
            next_blockptr = (void*)best_fit + (whole_block );
            next_blockptr->size_status = cushion - sizeof(block_header);

            //not at end of list
            if(NULL != best_fit->next ) {
                next_blockptr->next = best_fit->next;
            }else{//at end of list
                next_blockptr->next = NULL;
            }

            best_fit->next = next_blockptr;
        }

        //mark allocated
        best_fit->size_status++;
    }

    if(!found) {
        return NULL;
    }
    return best_fit + sizeof(block_header);
}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
    // Check if the pointer is pointing to the start of the payload of an allocated block
    if(NULL == ptr){
        return -1;
    }

    //fields
    block_header *free = (block_header*) ptr;
    free -= 8;
    block_header *next;
    next = free->next;

    if(isFree(free)){
        return -1;
    }

    //set as free
    free->size_status--;

    //next block coalece?
    if(NULL != next ){
        if(isFree(next)){
            free->size_status += (next->size_status + sizeof(block_header));
            free->next = next->next;
        }
    }

    //check to see if previous block is free as well
    block_header *itr = list_head;

    //if freed first block just return
    if(itr == free){
        return 0;
    }

    //traverse list, if previous block is free combine
    while(NULL != itr) {
        if (itr->next == free) {
            if (isFree(itr)) {
                itr->size_status += free->size_status + sizeof(block_header);
                itr->next = free->next;
                break;
            }
        }
        itr = itr->next;
    }
    return 0;
}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
    int counter;
    block_header* current = NULL;
    char* t_Begin = NULL;
    char* Begin = NULL;
    int Size;
    int t_Size;
    char* End = NULL;
    int free_size;
    int busy_size;
    int total_size;
    char status[5];

    free_size = 0;
    busy_size = 0;
    total_size = 0;
    current = list_head;
    counter = 1;
    fprintf(stdout,"************************************Block list***********************************\n");
    fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
    fprintf(stdout,"---------------------------------------------------------------------------------\n");
    while(NULL != current)
    {
        t_Begin = (char*)current;
        Begin = t_Begin + (int)sizeof(block_header);
        Size = current->size_status;
        strcpy(status,"Free");
        if(Size & 1) /*LSB = 1 => busy block*/
        {
            strcpy(status,"Busy");
            Size = Size - 1; /*Minus one for ignoring status in busy block*/
            t_Size = Size + (int)sizeof(block_header);
            busy_size = busy_size + t_Size;
        }
        else
        {
            t_Size = Size + (int)sizeof(block_header);
            free_size = free_size + t_Size;
        }
        End = Begin + Size - 1;
        fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
        total_size = total_size + t_Size;
        current = current->next;
        counter = counter + 1;
    }
    fprintf(stdout,"---------------------------------------------------------------------------------\n");
    fprintf(stdout,"*********************************************************************************\n");

    fprintf(stdout,"Total busy size = %d\n",busy_size);
    fprintf(stdout,"Total free size = %d\n",free_size);
    fprintf(stdout,"Total size = %d\n",busy_size+free_size);
    fprintf(stdout,"*********************************************************************************\n");
    fflush(stdout);
    return;
}
