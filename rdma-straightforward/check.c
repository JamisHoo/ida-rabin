#include "ec/ec-method.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

#include "nodes.h"

uint8_t* decoded;
uint8_t* data;
uint8_t* output[NUM_BRICKS];
uint32_t row[NUM_BRICKS];

void init(){
    int i;
    FILE* f;
    char filename[128];
    size_t size;

    decoded = (uint8_t *)malloc(DATA_SIZE);
    data = (uint8_t *)malloc(DATA_SIZE);

    for(i = 0;i < NUM_BRICKS; ++i) {
        output[i] = (uint8_t*)malloc(DATA_SIZE / COLUMN);
        row[i] = i;

        sprintf(filename, "output%d", i);
        f = fopen(filename, "rb");
        size = fread(output[i], DATA_SIZE / COLUMN, 1, f);
        assert(size == 1);
        fclose(f);
    }
    ec_method_initialize();
    
    f = fopen("data", "rb");
    size = fread(data, DATA_SIZE, 1, f);
    assert(size == 1);
    fclose(f);
}

int main(int argc, char** argv){
    int i, j, times;
    size_t size;

    init();
    printf("Finish init\n");

    size = DATA_SIZE / COLUMN;
    ec_method_decode(size, COLUMN, row, output, decoded);


    if (memcmp(data, decoded, DATA_SIZE)) 
        printf("wrong! \n");
    else 
        printf("right. \n");

    return 0;
}
