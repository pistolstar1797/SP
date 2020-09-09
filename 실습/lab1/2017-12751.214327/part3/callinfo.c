#include <stdlib.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <string.h>

int get_callinfo(char *fname, size_t fnlen, unsigned long long *ofs)
{
    unw_context_t context;
    unw_cursor_t cursor;
    unw_word_t off;
    char procname[256];
    int ret = 0;

    if(unw_getcontext(&context)) {
        return -1;
    }
                           
    if(unw_init_local(&cursor, &context)) {
        return -1;
    }   

    while(unw_step(&cursor) > 0) {
        if(unw_get_proc_name(&cursor, procname, 256, &off)==0) {
            strcpy(fname, procname);
            *ofs = off - 5;
            if(strcmp(fname, "main") == 0){
                break;
            }
        }
    }

    return 0;
}
