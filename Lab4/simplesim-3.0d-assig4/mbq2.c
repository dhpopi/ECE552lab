#include<stdlib.h>
#include<stdio.h>
#include <time.h>

int main(){
    int test[1000000];
    int stride = 128;
    
    int i = 0;
    for(i = 0; i < 1000000; i = i + stride){
        test[i] = 1;
        
        
        if(i % 2){
            stride = 64;
        }else{
            stride = 128;
        }
        //uncomment this line for fixing the stride, we should have miss rate = 0; if we have alternating stride, we should have higher missrate
        // stride = 10;
    }
    return 0;
}