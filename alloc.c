#include <assert.h>
#include <stdlib.h>
#include "mem.h"
#include <stdio.h>



int main() {
    Mem_Init(4096);

    void* ptr = Mem_Alloc(8);
    if(ptr != NULL){
        printf("sucessv\n");
    }else{
        printf("fail\n");
    }
    exit(0);
}

