#include<stdlib.h>
#include<stdio.h>

int main(){
    int test[1000000];
    int i = 0;
    //change this to 1, the cache miss rate will got 0, which means the prefetcher nextline is correct, 
    //if goes to higher stepping, we should have higher miss rate
    for(i = 0; i < 1000000; i = i + 1){
        test[i] = 1;
    }
    return 0;
}