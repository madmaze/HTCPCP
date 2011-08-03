#include <time.h>
#include <string.h>
#include <ctype.h>

//for testing
#include <stdio.h>


#define BUFFSIZE 4096
#define ERROR	0 
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

//Return codes
#define C_POT_BUSY "HTCPCP/1.0 510 Pot busy \r\n "
#define C_INVALID_ADDS "HTCPCP/1.0 406 Not Acceptable \r\nAdditions-List:CREAM;HALF-AND-HALF;WHOLE-MILK;PART-SKIM;SKIM;NON-DAIRY;VANILLA;ALMOND;RASPBERRY;CHOCOLATE;WHISKY;RUM;KAHLUA;AQUAVIT\r\n" 
#define C_BREW_SUC "HTCPCP/1.0 200 OK \r\nContent-length: 0\r\nContent-type: message/coffeepot \r\nETA: 30\r\n" 
#define C_NO_CUP "HTCPCP/1.0 511 No cup \r\n "
#define C_STILL_BREW "HTCPCP/1.0 505 Still Brewing \r\n "
#define C_ALREADY_ADD "HTCPCP/1.0 506 Already Added \r\n "
#define C_CUP_COLD "HTCPCP/1.0 419 Coffee gone cold \r\n" 
#define C_OVERFLOW "HTCPCP/1.0 420 Overflow error \r\n"
#define C_POURING "HTCPCP/1.0 200 OK \r\nContent-type: message/coffeepot \r\n\r\nSay WHEN \r\n"

char validAdditions[] = "CREAM;HALF-AND-HALF;WHOLE-MILK;PART-SKIM;SKIM;NON-DAIRY;VANILLA;ALMOND;RASPBERRY;CHOCOLATE;WHISKY;RUM;KAHLUA;AQUAVIT";

typedef struct {
	time_t finBrew;
	int cupWaiting;
	int additionsAdded;
	int addUnitsPerSec;
	time_t startPour;
	char waitingAdditions[20][255];
} potStruct;

const char *mystristr(const char *haystack, const char *needle)
{
	if ( !*needle )
	{
		return haystack;
	}
	for ( ; *haystack; ++haystack )
	{
		if ( toupper(*haystack) == toupper(*needle) )
		{
			/*
			 *           * Matched starting char -- loop through remaining chars.
			 *                     */
			const char *h, *n;
			for ( h = haystack, n = needle; *h && *n; ++h, ++n )
			{
				if ( toupper(*h) != toupper(*n) )
				{
					break;
				}
			}
			if ( !*n ) /* matched all of 'needle' to null termination */
			{
				return haystack; /* return the start of the match */
			}
		}
	}
	return 0;
}

//constructor for our pot struct.
void resetPot(potStruct * pot) {
	int i;

	pot->finBrew=0;
	pot->cupWaiting=FALSE;
	pot->additionsAdded=FALSE;
	pot->addUnitsPerSec=0;
	pot->startPour=0;

	for(i=0;i < 20; i++) {
		strcpy(pot->waitingAdditions[i], "");
	}
}

int getState(potStruct * pot) {
	time_t curTime = time(NULL);

	if(pot->cupWaiting == FALSE) {
		return(READY);
	}
	else if(pot->finBrew > curTime) {
		return(BREWING);
	}
	else if(pot->finBrew <= curTime) {
		if(difftime(curTime, pot->finBrew) >= SECS_TO_COLD) {
			return(CUP_COLD);
		}
		else if (pot->startPour <= curTime && pot->startPour > 0) {
			if((((int)difftime(curTime, pot->startPour))*pot->addUnitsPerSec) > FREE_UNITS_FOR_ADDS) {
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

int validateAdds(char additions[][255]) {
	char * tok;
	int i;

	for(i=0; i < 20; i++) {
		tok=strtok(additions[i], ";");
		if(tok == NULL) {
			return(TRUE);
		}
		if(mystristr(validAdditions,tok) == NULL) {
			return(FALSE);
		}
	}


}

int calcAddPerSec(char additions[][255]) {
	char * tok;
	int i;
	int count=0;

	for(i=0; i < 20; i++) {
		tok=strtok(additions[i], ";");
		tok=strtok(NULL,"");
		if(tok == NULL)
			return(count);
		count+=atoi(tok);
	}
	return(count);

}

void brew(potStruct * pot, char additions[][255], char * buf) {
	int potStatus=getState(pot);
	int i;

	if(potStatus != READY) {
		strcpy(buf,C_POT_BUSY);	
		return;
	}

	if(validateAdds(additions) == FALSE) {
		strcpy(buf,C_INVALID_ADDS);
		return;
	}

	strcpy(buf, C_BREW_SUC);
	resetPot(pot);

	pot->finBrew=time(NULL)+30;

	for(i=0; i < 20; i++) {
		strcpy(pot->waitingAdditions[i],additions[i]);
	}

	pot->addUnitsPerSec=calcAddPerSec(pot->waitingAdditions);

	pot->cupWaiting=TRUE;

	return;

}

void put(potStruct * pot, char * buf) {
	int potStatus=getState(pot);

	if(potStatus == READY) {
		strcpy(buf, C_NO_CUP);
		return;
	}

	if(potStatus == BREWING) {
		strcpy(buf, C_STILL_BREW);
		return;
	}

	if(potStatus == CUP_COLD) {
		strcpy(buf, C_CUP_COLD);
		resetPot(pot);
		return;
	}

	if(potStatus == CUP_WAITING_ADDS) {
		strcpy(buf, C_ALREADY_ADD);
		return;
	}

	if(potStatus == CUP_OVERFLOW) {
		strcpy(buf, C_OVERFLOW);
		resetPot(pot);
		return;
	}

	if(potStatus == POURING || potStatus == CUP_WAITING_NO_ADDS) {
		strcpy(buf, C_POURING);
		if(potStatus == CUP_WAITING_NO_ADDS) {
			pot->additionsAdded=FALSE;
			pot->startPour=time(NULL);
		}
	}

}

void when(potStruct * pot, char * buf) {
	in potStatus=getState(pot);



}

int main() {

	potStruct pot;

	int result;
	char test[BUFFSIZE];
	char twoTest[20][255];

	strcpy(twoTest[0], "Milk;1");
	strcpy(twoTest[1], "Rum;3");
	
	resetPot(&pot);
	result=calcAddPerSec(twoTest);
	brew(&pot, twoTest, test);
	//resetPot(&pot);

	/*	pot.cupWaiting=TRUE;
		pot.finBrew=time(NULL)-50;
		pot.addUnitsPerSec=1;
		pot.startPour=time(NULL)-5;
		pot.additionsAdded=TRUE;*/
	//result=getState(&pot);

	printf("Result: %d\n", result);
	printf("Buf: %s\n", test);

}
