/*
 * sntpTime.h
 *
 *  Created on: 7 Jun 2024
 *      Author: macra
 */


#ifndef sntpTime
#define sntpTime

//Change the timeZone
void sntpTimeChangeTimeZone(char* timezone);

//Starts the SNTP server syncronisation task
void sntpTimeTaskStart(void);

//Returns local time if set.
//@return local time buffer
char* sntpGetTIme(void);

bool isTimeValid(void);


#endif /* sntpTime */
