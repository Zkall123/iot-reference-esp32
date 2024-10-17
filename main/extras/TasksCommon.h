/*
 * TasksKommon.h
 *
 *  Created on: 17 Jun 2024
 *      Author: macra
 */

#ifndef MAIN_TASKSCOMMON_H_
#define MAIN_TASKSCOMMON_H_

//LED Application Task
#define LEDTaskStackSize						4096
#define LEDTaskPriority							5
#define LEDTaskCoreID							0

//PrintTask
#define PrintTaskStackSize						4096
#define PrintTaskPriority						3
#define PrintTaskCoreID							1

//SNTP Time Task
#define SNTPTaskStackSize						4096
#define SNTPTaskPriority						2
#define SNTPTaskCoreID							1

//OTA Task
#define OTATaskStackSize						8192
#define OTATaskPriority						    8

//Access Task
#define AccessTaskStackSize						3072
#define AccessTaskPriority					    7

//HasAccess Task
#define HasAccessTaskStackSize					3072
#define HasAccessTaskPriority				    7

//Settings Task
#define SettingsTaskStackSize					4096
#define SettingsTaskPriority				    7

//Scanning Task
#define ScanningTaskStackSize					2048
#define ScanningTaskPriority					7

//MakeSound Task
#define MakeSoundTaskStackSize					4096
#define MakeSoundTaskPriority				    7

#endif /* MAIN_TASKSCOMMON_H_ */
