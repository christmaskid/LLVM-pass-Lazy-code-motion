#include <stdio.h>

int main()
{ 
    int s = 0, a = 5;

    for (int i=0; i<a; i++) {
        s = 10000;
        int t = s;
        printf("%d\n", t);
    }

	return 0;
}