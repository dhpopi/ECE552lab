#include <stdio.h>

int main(int argc, char *argv[])
{
    // store the variables to registers, so that we will not load value from stack
    register int reg1 = 0;
    register int reg2 = 0;
    register int reg3 = 0;

    // does not store counter i in order to have load and store from stack opertaions instrution for variety
    int i;
    for (i = 0; i < 10000000; i++)
    {
        register1 = 1;
        register2 = 1;
        register3 = register1 + register2;
    }

    return 0;
}