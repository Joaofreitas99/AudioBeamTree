#include<stdio.h>
#include<stdlib.h>
#define BUFMAX 100

int main() {
    char buffer[BUFMAX + 1];
    char *bp = buffer;
    int c;
    FILE *in;
    while (EOF != (c = fgetc(stdin)) && (bp - buffer) < BUFMAX) {
        *bp++ = c;
    }
    *bp = 0;    // Null-terminate the string
    printf("%s", buffer);
}