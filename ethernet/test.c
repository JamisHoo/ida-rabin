#include "ec-method.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#define DATA_SIZE (2016ll << 20)
#define COLUMN (48)
#define ROW (COLUMN + 16)

uint8_t *decoded,*data;
uint8_t * output[ROW];
uint32_t row[ROW];

void init(){
    int i;
    decoded = (uint8_t *)malloc(DATA_SIZE);
    data = (uint8_t *)malloc(DATA_SIZE);

    for(i=0;i<ROW;i++)
        output[i] = (uint8_t *)malloc(DATA_SIZE/COLUMN),row[i]=i;
    ec_method_initialize();
    

   for(i=0;i<DATA_SIZE;i++)
        data[i] = rand()%256;
}

int main(int argc,char *argv[]){
    int i,j,times;
    size_t size;
    struct timeval begin,end,result;
    double total_time;


    init();
    printf("Finish init\n");

    gettimeofday(&begin,NULL);

    if(argc > 1 && !strcmp(argv[1],"-p"))
        size = ec_method_batch_parallel_encode(DATA_SIZE, COLUMN, ROW, data, output,get_nprocs());
     else
        size = ec_method_batch_encode(DATA_SIZE, COLUMN, ROW, data, output);

    gettimeofday(&end,NULL);
    timersub(&end,&begin,&result);

    printf("%sencode cost:%ld.%06lds\n",(argc>1 && !strcmp(argv[1],"-p")?"parallel ":""),result.tv_sec,result.tv_usec);

    /*
    for(i=0;i<100;i++){
        printf("%d :",i*10);
        for(j=0;j<10;j++)
            printf("%x:%x ",output[1][i*10+j],output[2][i*10+j]);
        printf("\n");
    }
    */

    gettimeofday(&begin,NULL);
    if(argc > 1 && !strcmp(argv[1],"-p"))
        ec_method_parallel_decode(size, COLUMN, row, output, decoded,get_nprocs());
    else
        ec_method_decode(size,COLUMN,row,output,decoded);

    gettimeofday(&end,NULL);
    timersub(&end,&begin,&result);

    printf("%sdecode cost:%ld.%06lds\n",(argc>1 && !strcmp(argv[1],"-p")?"parallel ":""),result.tv_sec,result.tv_usec);

    /*
    for(i=0;i<10;i++){
        printf("%d :",i*10);
        for(j=0;j<10;j++)
            printf("%x:%x ",data[i*10+j],decoded[i*10+j]);
        printf("\n");
    }
    */

    if (memcmp(data, decoded, DATA_SIZE)) 
        printf("wrong! \n");
    else 
        printf("right. \n");



    return 0;
}
