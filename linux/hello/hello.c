// Compile with 
//    gcc -o ../bin/hello -nostdlib hello.c

#include <syscall.h>

void syscall1(int num, int arg1)
{
    asm("int\t$0x80\n\t":
        /* output */    :
        /* input  */    "a"(num), "b"(arg1)
        /* clobbered */ );
}

void syscall2(int num, int arg1, int arg2)
{
    asm("int\t$0x80\n\t" :
        /* output */     :
        /* input  */     "a"(num), "b"(arg1), "c"(arg2)
        /* clobbered */ );
}

void syscall3(int num, int arg1, int arg2, int arg3)
{
    asm("int\t$0x80\n\t" :
        /* output */     :
        /* input  */     "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) 
        /* clobbered */ );
}

int strlen(char* str)
{
    int i = 0;
    while(str[i++] != '\0');
    return i-1;
}

void print(char* str)
{
    syscall3(SYS_write, 1, (int)str, strlen(str));
}

void exit(int code)
{
    syscall1(SYS_exit, code);
}

void _start()
{
    print("Hello, world!\n");
    exit(0);
}
