#include <stdio.h>

int main()
{ 
	int r1 = 1, r2 = 2, r3;

	if (r1 < 5) {
		r3 = r1 + r2;
        r1++;
        r2++;
		printf("%d\n", r3);
	}
	else {
		printf("%d\n", r1);
	}
    printf("%d%d\n", r1, r2);

	return 0;
}