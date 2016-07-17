#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define SYSTEM_MALLOC 0
#define DEFAULT_SBRK 4096
#define FREE_FLAG 0
#define END_FLAG 2
#define INUSE_FLAG 1

int max(int a, int b){
    return ((a > b) ? a : b);
}

struct header {
  unsigned short free;
  struct header *next;
};

static const short W_SIZE = (sizeof (void *));
static const short H_SIZE = (sizeof (struct header));
static struct header *start = NULL;
static long heap_size = 0;

static pthread_mutex_t lock;

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
    void * ret = sbrk(size + 3*H_SIZE);
    if (ret == (void *)(-1))
      return NULL;
    start = ret;
    heap_size = size + H_SIZE;
    start -> free = INUSE_FLAG;

    //adding the header for the last (empty) block indicating the end of heap
    struct header * temp = (struct header * )(((char *) ret) + size + 2*H_SIZE);
    temp -> free = END_FLAG;
    temp -> next = temp;

    start -> next = temp;
    

    ret = (void *) (start + 1); //set return value and release lock
    pthread_mutex_unlock(&lock);
    return ret;
  }

  struct header * temp = start;

  //iterate through headers, checking for a large enough free block
  while (temp != temp->next){
    if (temp ->free == FREE_FLAG){
      ptrdiff_t freeSpace = temp->next - temp; //measure dist between headers
      freeSpace = freeSpace*H_SIZE;

      if (freeSpace > size + H_SIZE){  //if there is a sufficient free block
        temp->free = INUSE_FLAG;
        void * ret = (void *)(temp + 1);
        pthread_mutex_unlock(&lock);
        return ret;
      }
    }
    temp = temp->next;
  }

  //No free block large enough, so sbrk the heap
  void * ret = sbrk(max(size + 3*H_SIZE, DEFAULT_SBRK));
  if (ret == (void *)(-1))
    return NULL;

  temp ->free = INUSE_FLAG;
  temp-> next = sbrk(0)-H_SIZE;
  ret = temp + 1;

  temp = temp -> next;
  temp->free = END_FLAG;
  temp->next = temp;

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
	
    //no need for a lock since we are not combining free blocks
    struct header * temp = ptr;
    temp = temp - 1;
	temp -> free = FREE_FLAG;
	return 0;
}
