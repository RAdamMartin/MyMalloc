#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define SYSTEM_MALLOC 0

#define DEBUG 0
#define FREE_DEBUG 0

/* Credit: 
 * http://stackoverflow.com/questions/1644868/c-define-macro-for-debug-printing 
 */
#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stdout, fmt, __VA_ARGS__); } while (0)
#define fdebug_print(fmt, ...) \
            do { if (FREE_DEBUG) fprintf(stdout, fmt, __VA_ARGS__); } while (0)

struct header {
  struct header *free; //NULL if in use, else pointer to another free block
  struct header *next; //Pointer to next block in heap
};

static const short H_SIZE = (sizeof (struct header));
static struct header *start = NULL;  //Header for first block in heap
static struct header *end = NULL;  //Header for empty block at end of heap
static struct header *freeHead = NULL; //Header for an available free block, 
                                      //heading an unordered list of free blocks
static pthread_mutex_t lock;


/* Removes a block from the list of free blocks, or does nothing if DNE
 */
int removeFromFree(struct header *target){
  fdebug_print("Removing %p from frees\n", target);
  if (freeHead == target){
    freeHead = freeHead->free;
  }
  else{
    struct header * prev = freeHead;
    while (prev->free != target){
      prev = prev ->free;
    }
    prev->free = prev->free->free;
  }
  return 0;
}

/* Checks if the preceding block is free, coalescing and returning 1 if so,
   otherwise returning 0
*/
int checkPrev(struct header *new){
  struct header * temp = freeHead;
  while (temp != NULL){
    if (temp->next == new){
      fdebug_print("Found free block %p preceding %p \n", temp, new);
      temp->next = temp->next->next;
      return 1;
    }
    temp = temp->free;
  }
  return 0;
}

/* mymalloc: allocates memory on the heap of the requested size. The block
             of memory returned should always be padded so that it begins
             and ends on a word boundary.
     unsigned int size: the number of bytes to allocate.
     retval: a pointer to the block of memory allocated or NULL if the 
             memory could not be allocated. 
             (NOTE: the system also sets errno, but we are not the system, 
                    so you are not required to do so.)
*/
void *mymalloc(unsigned int size) {
#if SYSTEM_MALLOC
  return malloc(size);
#endif
  
   pthread_mutex_lock(&lock);  //Lock for entire allocation

   //Initial case, setting up the head of the linked list
   if (start == NULL){
     int allocated = size + 3*H_SIZE - size%H_SIZE;
     void * ret = sbrk(allocated);
     //Check if out of memory
     if (ret == (void *)(-1)){
       pthread_mutex_unlock(&lock);
       return NULL;
     }

     start = ret;
     start -> free = NULL;

     end = start + allocated/H_SIZE - 1;
     end -> free = NULL;
     end -> next = end;

     start -> next = end;

     ret = (void *) (start + 1); //set return value and release lock
     debug_print("\tStarting at %p \n", start);
     pthread_mutex_unlock(&lock);
     return ret;
   }

  struct header * temp = freeHead; //Get 'first' free block
  struct header * best = NULL; 
  ptrdiff_t bestFit = 0;
  ptrdiff_t currFit = 0;

  //Loop through free blocks looking for best fit
  while (temp != NULL){
    currFit = (temp->next - temp - 1) * H_SIZE;
    if (currFit > size && (bestFit == 0 || currFit < bestFit)){
      bestFit = currFit;
      best = temp;
      if (bestFit < size + H_SIZE)
        break;
    }
    temp = temp->free;
  }
  
  //Found a free block, remove blocck from freelist and return it
  if (best != NULL){
    fdebug_print("Found fit of size %lu for size %d at %p \n", 
                 bestFit, size, best);
    removeFromFree(best);
    best ->free = NULL;
    void * ret = (void *) (best + 1);
    pthread_mutex_unlock(&lock);
    return ret;
  } 

  //No free block large enough, so sbrk the heap
  int allocated = size - size%H_SIZE + 2*H_SIZE;
  void * ret = sbrk(allocated);

  if (ret == (void *)(-1)){
    pthread_mutex_unlock(&lock);
    return NULL;
  }

  //Set temp = to the current end of heap and move the end pointer to new end
  temp = end;
  temp ->free = NULL;
  temp-> next = ((struct header *)sbrk(0))-1;
  end = temp -> next;
  end->free = NULL;
  end->next = end;

  ret = (void *)(temp + 1);
  debug_print("\tProvided %p, new end: %p \n", ret, end);
  pthread_mutex_unlock(&lock);

  return ret;
}

/* myfree: unallocates memory that has been allocated with mymalloc.
     void *ptr: pointer to the first byte of a block of memory allocated by 
                mymalloc.
     retval: 0 if the memory was successfully freed and 1 otherwise.
             (NOTE: the system version of free returns no error.)
*/
unsigned int myfree(void *ptr) {
	#if SYSTEM_MALLOC
	free(ptr);
	return 0;
	#endif
	
    if (ptr == NULL) //Check if valid ptr
      return 1;
    
    struct header * temp = ptr;
    temp = temp - 1;

    //lock for freeing so that free list cannot be allocated during free
    pthread_mutex_lock(&lock);

    if (temp -> free != NULL)//Check if ptr has already been freed
      return 1;

    fdebug_print("Freeing %p, headed at %p, with freeHead %p \n",
                 ptr, temp, freeHead);

    while (temp->next->free != NULL){ //Check following blocks for coalescing
      fdebug_print("Folding %p into %p and ", temp->next, temp);
      removeFromFree(temp->next);
      temp->next = temp->next->next;
    }

    //Check if preceding block is free, if not add temp to free list at head
    if(!checkPrev(temp)){
      temp->free = freeHead;
      freeHead = temp;
    }

    debug_print("\t Freed %p \n", temp);
#if FREE_DEBUG
    temp = freeHead;
    while (temp != NULL){
      printf("%p ->", temp);
      temp = temp ->free;
    }
    printf("\n freeHead->next = %p \n --- \n", freeHead->next);
#endif

    pthread_mutex_unlock(&lock);
	return 0;
}
