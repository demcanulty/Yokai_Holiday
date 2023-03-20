#include <stdio.h>
#include <stdint.h>

float crossfade(float a, float b, float p) {
	return a + (b - a) * p;
}


int main(int argc, char *argv[]) 
{
	float out = crossfade(2, 8, 0.f);
	
	printf("%f", out);
	
	return 0;
}