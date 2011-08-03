#include <time.h>


#define TRUE	1
#define FALSE 	0


char additions[] = "CREAM;HALF-AND-HALF;WHOLE-MILK;PART-SKIM;SKIM;NON-DAIRY;VANILLA;ALMOND;RASPBERRY;CHOCOLATE;WHISKY;RUM;KAHLUA;AQUAVIT";

typedef struct {
	time_t finBrew;
	int cupWaiting;
	int additionsAdded;
	time_t startPour;
	char waitingAdditions[255];
} potStruct;
