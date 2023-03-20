#include <stdio.h>
#include <stdint.h>

#define BSIZE      0x20000
#define FADE_SIZE  1024

uint32_t get_trail_pointer_from_distance(uint32_t this_trailptr, uint32_t this_distance)
{
	uint32_t out = (this_trailptr - this_distance) & (BSIZE -1);
	
	return out;
}



int main(int argc, char *argv[]) 
{
	uint32_t trailptr;
	uint32_t out;
	uint32_t distance;
	
	trailptr = 30;
	
	for(int i=0; i <(FADE_SIZE); i+=2)
	{
		trailptr++;  
		trailptr &= (BSIZE -1);
		out = get_trail_pointer_from_distance(trailptr, i);
		printf("%d \t%d \t%d \n", trailptr, i, out);
	}
//	trailptr = 120;
//	 distance = 823;
//	out = (trailptr - distance);
//	printf("%d \t%d \n", out, out & 1023);
	
	
	return 0;
}