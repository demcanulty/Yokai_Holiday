#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) 
{
	int fixed_index, index;
	
	fixed_index = 3;
	index = 40;
	
	for(int i =0; i<1000; i++)
	{
		int ptr_trail_distance = (fixed_index - index);
		printf("%d %d %d\n", i, index, ptr_trail_distance % 96000);
		
		index--;
		if(index < 0)
		{
			index = 96000;
		}
	}
	
	return 0;
}