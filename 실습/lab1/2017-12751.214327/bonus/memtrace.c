//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>
#include "callinfo.h"


//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

void *malloc(size_t size)
{
	char *error;
	void *ptr;

	if(!mallocp) {
		mallocp = dlsym(RTLD_NEXT, "malloc");
		if ((error = dlerror()) != NULL) {
			mlog(0, error);
			exit(1);
		}
    }

    LOG_MALLOC(size, ptr);
	
	ptr = mallocp(size);
    n_allocb += size;
    n_malloc++;
    alloc(list, ptr, size);
    return ptr;
}

void *calloc(size_t nmemb, size_t size)
{
    char *error;
    void *ptr;
    
    if(!callocp) {
        callocp = dlsym(RTLD_NEXT, "calloc");
        if ((error = dlerror()) != NULL) {
            mlog(0, error);
            exit(1);
        }
    }

    LOG_CALLOC(nmemb, size, ptr);

    ptr = callocp(nmemb, size);
    n_allocb += nmemb * size;
    n_calloc++;
    alloc(list, ptr, nmemb*size);
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    char *error;
    void *ptr2;

    if(!reallocp) {
        reallocp = dlsym(RTLD_NEXT, "realloc");
        if((error = dlerror()) != NULL) {
            mlog(0, error);
            exit(1);
        }
    }

    LOG_REALLOC(ptr, size, ptr2);

    if(find(list, ptr) == NULL) {
        LOG_ILL_FREE();
        ptr2 = reallocp(NULL, size);
    }
    else if(find(list, ptr)->cnt == 0) {
        LOG_DOUBLE_FREE();
        ptr2 = reallocp(NULL, size);
    }
    else {
        ptr2 = reallocp(ptr, size);
        n_freeb += find(list, ptr)->size;
        dealloc(list, ptr);
    }
    n_allocb += size;
    n_realloc++;
    alloc(list, ptr2, size);
    return ptr2;

}

void free(void *ptr)
{
    char *error;
    
    if(!freep) {
        freep = dlsym(RTLD_NEXT, "free");
        if((error = dlerror()) != NULL) {
            mlog(0, error);
            exit(1);
        }
    }

    LOG_FREE(ptr);

    if(find(list, ptr) == NULL) {
        LOG_ILL_FREE();
    }
    else if(find(list, ptr)->cnt == 0) {
        LOG_DOUBLE_FREE();
    }
    else {
        freep(ptr);
        n_freeb += find(list, ptr)->size;
        dealloc(list, ptr);
    }
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  LOG_STATISTICS(n_allocb, n_allocb / (n_malloc + n_calloc + n_realloc), n_freeb);

    item *i = list->next;
    int check = 0;

    while(i != NULL) {
        if(i->cnt > 0) {
            if(check == 0) {
                LOG_NONFREED_START();
                check = 1;
            }
            LOG_BLOCK(i->ptr, i->size, i->cnt, i->fname, i->ofs);
        }
        
        i = i->next;
    }

  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...
