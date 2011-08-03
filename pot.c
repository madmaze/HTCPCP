#include <time.h>
#include <string.h>

//for testing


#define BUFFSIZE 4096
#define ERROR -1
#define TRUE	1
#define FALSE 	0
#define READY	1
#define BREWING	2
#define CUP_WAITING_NO_ADDS	3
#define CUP_WAITING_ADDS	4
#define POURING	5
#define CUP_OVERFLOW	6
#define CUP_COLD	7

#define SECS_TO_COLD	60
#define FREE_UNITS_FOR_ADDS	100

char additions[] = "CREAM;HALF-AND-HALF;WHOLE-MILK;PART-SKIM;SKIM;NON-DAIRY;VANILLA;ALMOND;RASPBERRY;CHOCOLATE;WHISKY;RUM;KAHLUA;AQUAVIT";

typedef struct {
	time_t finBrew;
	int cupWaiting;
	int additionsAdded;
	int addUnitsPerSec;
	time_t startPour;
	char waitingAdditions[255];
} potStruct;


//constructor for our pot struct.
void resetPot(potStruct * pot) {
	pot->finBrew=0;
	pot->cupWaiting=FALSE;
	pot->additionsAdded=FALSE;
	pot->addUnitsPerSec=0;
	pot->startPour=0;
	strcpy(pot->waitingAdditions, "");
}

int getState(potStruct * pot) {
	time_t curTime = time(NULL);

	if(pot->cupWaiting == FALSE) {
		return(READY);
	}
	else if(pot->finBrew < curTime) {
		return(BREWING);
	}
	else if(pot->finBrew >= curTime) {
		if(difftime(pot->finBrew, curTime) >= SECS_TO_COLD) {
			return(CUP_COLD);
		}
		else if (pot->startPour >= curTime) {
			if((((int)difftime(pot->startPour, curTime))*FREE_UNITS_FOR_ADDS) > 100) {
				return(CUP_OVERFLOW);
			}
			else if (pot->additionsAdded == TRUE) {
				return(CUP_WAITING_ADDS);
			}
			else if (pot->additionsAdded == FALSE) {
				return(POURING);
			}
		}
		else if (pot->startPour == 0) {
			return(CUP_WAITING_NO_ADDS);
		}

	}
	else {
		return(ERROR);
	}

}

int main() {

	potStruct pot;

	int result;

	resetPot(&pot);

	result=getState(&pot);

	printf("Result: %d\n" result);

}
