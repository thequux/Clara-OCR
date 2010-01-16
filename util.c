#include <stdlib.h>
#include <execinfo.h>
#include <stdio.h>

void UNIMPLEMENTED() {
        void* symlocs[20];
        char** syms;
        int count,i;
        count = backtrace(symlocs,20);
        fprintf(stderr, "\nFunction not implemented. Backtrace:\n");
        syms = backtrace_symbols(symlocs, count);
        for (i = 0; i < count; i++)
                fprintf(stderr,"\t%3d: %s\n",i, syms[i]);
        fprintf(stderr,"Aborting\n");
        
        abort();
}
