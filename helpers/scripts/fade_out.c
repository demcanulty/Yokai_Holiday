#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) 
{
	float samplerate = 480;
	uint32_t distance = 0;
	
	for(int i=0; distance<samplerate; i++)
	{
		float xfade;
		
		xfade = (samplerate - distance ) / samplerate;
		
		printf("%6f\n", xfade);
		
		distance +=2 ;
		
		
	}
	
	
	return 0;
}