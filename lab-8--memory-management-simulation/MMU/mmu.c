#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "list.h"
#include "util.h"

void TOUPPER(char * arr){
  
    for(int i=0;i<strlen(arr);i++){
        arr[i] = toupper(arr[i]);
    }
}

void get_input(char *args[], int input[][2], int *n, int *size, int *policy) 
{
  	FILE *input_file = fopen(args[1], "r");
	  if (!input_file) {
		    fprintf(stderr, "Error: Invalid filepath\n");
		    fflush(stdout);
		    exit(0);
	  }

    parse_file(input_file, input, n, size);
  
    fclose(input_file);
  
    TOUPPER(args[2]);
  
    if((strcmp(args[2],"-F") == 0) || (strcmp(args[2],"-FIFO") == 0))
        *policy = 1;
    else if((strcmp(args[2],"-B") == 0) || (strcmp(args[2],"-BESTFIT") == 0))
        *policy = 2;
    else if((strcmp(args[2],"-W") == 0) || (strcmp(args[2],"-WORSTFIT") == 0))
        *policy = 3;
    else {
       printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
       exit(1);
    }
        
}

void allocate_memory(list_t * freelist, list_t * alloclist, int pid, int blocksize, int policy) {
    node_t *prev = NULL, *current = freelist->head, *selected_node = NULL, *selected_prev = NULL;
    int found = 0;

    // Search for a block based on the policy
    while (current != NULL) {
        int current_size = current->blk->end - current->blk->start + 1;

        if (current_size >= blocksize) {
            if (policy == 1) { // FIFO (First Fit)
                selected_node = current;
                selected_prev = prev;
                found = 1;
                break; // First fit, so stop after finding the first suitable block
            } else if (policy == 2) { // Best Fit
                if (!selected_node || current_size < (selected_node->blk->end - selected_node->blk->start + 1)) {
                    selected_node = current;
                    selected_prev = prev;
                    found = 1;
                }
            } else if (policy == 3) { // Worst Fit
                if (!selected_node || current_size > (selected_node->blk->end - selected_node->blk->start + 1)) {
                    selected_node = current;
                    selected_prev = prev;
                    found = 1;
                }
            }
        }

        prev = current;
        current = current->next;
    }

    if (!found) {
        printf("Error: Memory Allocation %d blocks\n", blocksize);
        return;
    }

    // Remove the selected node from the free list
    if (selected_prev == NULL) {
        freelist->head = selected_node->next;
    } else {
        selected_prev->next = selected_node->next;
    }

    // Allocate memory
    block_t *allocated_block = malloc(sizeof(block_t));
    allocated_block->pid = pid;
    allocated_block->start = selected_node->blk->start;
    allocated_block->end = allocated_block->start + blocksize - 1;

    list_add_ascending_by_address(alloclist, allocated_block);

    // Handle fragmentation
    int remaining_start = allocated_block->end + 1;
    if (remaining_start <= selected_node->blk->end) {
        block_t *fragment = malloc(sizeof(block_t));
        fragment->pid = 0;
        fragment->start = remaining_start;
        fragment->end = selected_node->blk->end;

        if (policy == 1) {
            list_add_to_back(freelist, fragment); // FIFO
        } else if (policy == 2) {
            list_add_ascending_by_blocksize(freelist, fragment); // Best Fit
        } else if (policy == 3) {
            list_add_descending_by_blocksize(freelist, fragment); // Worst Fit
        }
    }

    free(selected_node->blk);
    free(selected_node);

}

void deallocate_memory(list_t * alloclist, list_t * freelist, int pid, int policy) { 
    node_t *prev = NULL, *current = alloclist->head;

    // Search for the block with the given PID
    while (current != NULL && current->blk->pid != pid) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        printf("Error: Can't locate Memory Used by PID: %d\n", pid);
        return;
    }

    // Remove the block from the allocated list
    if (prev == NULL) {
        alloclist->head = current->next;
    } else {
        prev->next = current->next;
    }

    // Reset the PID and return the block to the free list
    current->blk->pid = 0;

    if (policy == 1) {
        list_add_to_back(freelist, current->blk); // FIFO
    } else if (policy == 2) {
        list_add_ascending_by_blocksize(freelist, current->blk); // Best Fit
    } else if (policy == 3) {
        list_add_descending_by_blocksize(freelist, current->blk); // Worst Fit
    }

    free(current);
}

list_t* coalese_memory(list_t * list){
  list_t *temp_list = list_alloc();
  block_t *blk;
  
  while((blk = list_remove_from_front(list)) != NULL) {  // sort the list in ascending order by address
        list_add_ascending_by_address(temp_list, blk);
  }
  
  // try to combine physically adjacent blocks
  node_t *current = temp_list->head, *next;
    while (current != NULL && current->next != NULL) {
        next = current->next;
        if (current->blk->end + 1 == next->blk->start) { // Check if blocks are adjacent
            current->blk->end = next->blk->end; // Merge blocks
            current->next = next->next;

            free(next->blk);
            free(next);
        } else {
            current = current->next;
        }
    }

  list_coalese_nodes(temp_list);
        
  return temp_list;
}

void print_list(list_t * list, char * message){
    node_t *current = list->head;
    block_t *blk;
    int i = 0;
  
    printf("%s:\n", message);
  
    while(current != NULL){
        blk = current->blk;
        printf("Block %d:\t START: %d\t END: %d", i, blk->start, blk->end);
      
        if(blk->pid != 0)
            printf("\t PID: %d\n", blk->pid);
        else  
            printf("\n");
      
        current = current->next;
        i += 1;
    }
}

/* DO NOT MODIFY */
int main(int argc, char *argv[]) 
{
   int PARTITION_SIZE, inputdata[200][2], N = 0, Memory_Mgt_Policy;
  
   list_t *FREE_LIST = list_alloc();   // list that holds all free blocks (PID is always zero)
   list_t *ALLOC_LIST = list_alloc();  // list that holds all allocated blocks
   int i;
  
   if(argc != 3) {
       printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
       exit(1);
   }
  
   get_input(argv, inputdata, &N, &PARTITION_SIZE, &Memory_Mgt_Policy);
  
   // Allocated the initial partition of size PARTITION_SIZE
   
   block_t * partition = malloc(sizeof(block_t));   // create the partition meta data
   partition->start = 0;
   partition->end = PARTITION_SIZE + partition->start - 1;
                                   
   list_add_to_front(FREE_LIST, partition);          // add partition to free list
                                   
   for(i = 0; i < N; i++) // loop through all the input data and simulate a memory management policy
   {
       printf("************************\n");
       if(inputdata[i][0] != -99999 && inputdata[i][0] > 0) {
             printf("ALLOCATE: %d FROM PID: %d\n", inputdata[i][1], inputdata[i][0]);
             allocate_memory(FREE_LIST, ALLOC_LIST, inputdata[i][0], inputdata[i][1], Memory_Mgt_Policy);
       }
       else if (inputdata[i][0] != -99999 && inputdata[i][0] < 0) {
             printf("DEALLOCATE MEM: PID %d\n", abs(inputdata[i][0]));
             deallocate_memory(ALLOC_LIST, FREE_LIST, abs(inputdata[i][0]), Memory_Mgt_Policy);
       }
       else {
             printf("COALESCE/COMPACT\n");
             FREE_LIST = coalese_memory(FREE_LIST);
       }   
     
       printf("************************\n");
       print_list(FREE_LIST, "Free Memory");
       print_list(ALLOC_LIST,"\nAllocated Memory");
       printf("\n\n");
   }
  
   list_free(FREE_LIST);
   list_free(ALLOC_LIST);
  
   return 0;
}