#include <stdlib.h>
#include <stdarg.h>
#include "stringlib.h"

#define BLOCK 10

char * copyToNewStringEndedIn(char * string, char ended) {
	size_t i = 0;
	char * newString = NULL;

	while(string[i] != ended)
	{	
		if(i % BLOCK == 0)
			newString = realloc(newString, i + BLOCK);
		newString[i] = string[i];
		i++;
	}	
	i++;
	newString = realloc(newString, i);
	newString[i] = 0;
	return newString;
}

