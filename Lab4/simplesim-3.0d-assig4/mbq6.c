#include<stdlib.h>
#include<stdio.h>
#include <time.h>

int main(){
    int test[1000000];
    register int stride = 128;
    int pat = 10;
    
    int i = 0;
    for(i = 0; i < 1000000; i = i + 1){
        
        test[i + stride] = 1;
        
         //comment this line for fixing the stride, we should have miss rate = 0; if we have alternating stride, we should have higher missrate
        if(1 % 10 < 3){
            stride = 10;
        }else{
            stride = 64;
        }
       
    }
    return 0;
}