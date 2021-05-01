// Kutay Cinar
// V00******

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

/*---------------------- Definitions ------------------------*/

#define PRIORITY_MAX 		4
#define PRIORITY_HIGH 		2
#define PRIORITY_MIN 		1

#define MONITOR				pdMS_TO_TICKS(333)

/*-------------------- DD Task Structs ---------------------*/

typedef enum task_type task_type;

enum task_type {
	PERIODIC,
	APERIODIC
};

typedef struct dd_task dd_task;

struct dd_task {
	TaskHandle_t t_handle;
	task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
	uint32_t period;
};

typedef struct dd_task_list dd_task_list;

struct dd_task_list {
	dd_task task;
	struct dd_task_list* next_task;
};

/*------------------ Message Structs -----------------------*/

typedef enum message_type message_type;

enum message_type {
	create,
	delete,
	active,
	completed,
	overdue,
};

typedef struct dd_message dd_message;

struct dd_message {
	message_type type;
	dd_task task;
};

/*---------------------- Queues -----------------------------*/

xQueueHandle xMessageQueue;
xQueueHandle xReplyQueue;
xQueueHandle xListQueue;

/*----------------- Task Prototypes -------------------------*/

TaskHandle_t xDDS;
TaskHandle_t xMonitor;

void DD_Scheduler(void* pvParameters);
void Monitor_Task(void* pvParameters);

/*--------------- Functions Prototypes ----------------------*/

void create_dd_task(TaskHandle_t t_handle,
	task_type type,
	uint32_t task_id,
	uint32_t absolute_deadline
);

void delete_dd_task(uint32_t task_id);

dd_task_list** get_active_dd_task_list(void);
dd_task_list** get_completed_dd_task_list(void);
dd_task_list** get_overdue_dd_task_list(void);

void myDDSInit(void);
void myTestInit(void);

void vCallbackTask1(TimerHandle_t xTimer);
void vCallbackTask2(TimerHandle_t xTimer);
void vCallbackTask3(TimerHandle_t xTimer);
void vCallbackMonitor(TimerHandle_t xTimer);

TimerHandle_t xTimerUser1;
TimerHandle_t xTimerUser2;
TimerHandle_t xTimerUser3;
TimerHandle_t xTimerMonitor;

/*--------------- Test Tasks Prototypes ---------------------*/

void vUserTask1(void* pvParameters);
void vUserTask2(void* pvParameters);
void vUserTask3(void* pvParameters);

void vTaskGenerator1(void* pvParameters);
void vTaskGenerator2(void* pvParameters);
void vTaskGenerator3(void* pvParameters);

/*---------------- Test Tasks Handles ----------------------*/

TaskHandle_t xUserTask1;
TaskHandle_t xUserTask2;
TaskHandle_t xUserTask3;

TaskHandle_t xTaskGenerator1;
TaskHandle_t xTaskGenerator2;
TaskHandle_t xTaskGenerator3;

/*----------------- Test Task Bench 1 -------------------------*/

#define T1_EXEC				pdMS_TO_TICKS(95)
#define T1_PERIOD			pdMS_TO_TICKS(500)

#define T2_EXEC				pdMS_TO_TICKS(150)
#define T2_PERIOD			pdMS_TO_TICKS(500)

#define T3_EXEC				pdMS_TO_TICKS(250)
#define T3_PERIOD			pdMS_TO_TICKS(750)

int expectedUser1[] = { 95, 595, 1095, -1 };
int expectedUser2[] = { 245, 745, 1245, -1 };
int expectedUser3[] = { 495, 1495, -1 };


/*----------------- Test Task Bench 2 -------------------------*/

//#define T1_EXEC				pdMS_TO_TICKS(95)
//#define T1_PERIOD			pdMS_TO_TICKS(250)
//
//#define T2_EXEC				pdMS_TO_TICKS(150)
//#define T2_PERIOD			pdMS_TO_TICKS(500)
//
//#define T3_EXEC				pdMS_TO_TICKS(250)
//#define T3_PERIOD			pdMS_TO_TICKS(750)
//
//int expectedUser1[] = {95, 345, 685, 930, 1095, -1 };
//int expectedUser2[] = {245, 835, 1245, -1 };
//int expectedUser3[] = {590, 1425, -1 };


/*----------------- Test Task Bench 3 -------------------------*/

//#define T1_EXEC				pdMS_TO_TICKS(100)
//#define T1_PERIOD			pdMS_TO_TICKS(500)
//
//#define T2_EXEC				pdMS_TO_TICKS(200)
//#define T2_PERIOD			pdMS_TO_TICKS(500)
//
//#define T3_EXEC				pdMS_TO_TICKS(200)
//#define T3_PERIOD			pdMS_TO_TICKS(500)
//
//int expectedUser1[] = { 100, 600, 1100, -1 };
//int expectedUser2[] = { 300, 800, 1300, -1 };
//int expectedUser3[] = { 500, 1000, 1500, -1 };

/*-----------------------------------------------------------*/

uint32_t taskID1 = 10000;
uint32_t taskID2 = 20000;
uint32_t taskID3 = 30000;

int eventid = 0;

/*-----------------------------------------------------------*/

void main_dds(void)
{
	/* Initialize DD Scheduler and Monitor Task */
	myDDSInit();

	/* Create the tasks from Test Bench (Choose from 1, 2 or 3) */
	myTestInit();

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	for (;;);
}

/*-----------------------------------------------------------*/

void myDDSInit()
{
	// Initialize DD Scheduler Message and Reply Queue
	xMessageQueue = xQueueCreate(100, sizeof(dd_message));
	xReplyQueue = xQueueCreate(100, sizeof(uint32_t));
	xListQueue = xQueueCreate(1, sizeof(dd_task_list**));

	// Create DD Scheduler with max priority
	xTaskCreate(DD_Scheduler, "DDS", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MAX, &xDDS);

	// Monitor Task and its timer
	xTaskCreate(Monitor_Task, "MON", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MAX - 1, &xMonitor);
	vTaskSuspend(xMonitor);
	xTimerMonitor = xTimerCreate("TMR_MON", pdMS_TO_TICKS(MONITOR), pdTRUE, 0, vCallbackMonitor);
	xTimerStart(xTimerMonitor, 0);
}

/*-----------------------------------------------------------*/

void myTestInit(void)
{
	xTaskCreate(vUserTask1, "T1", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MIN, &xUserTask1);
	vTaskSuspend(xUserTask1);
	xTaskCreate(vUserTask2, "T2", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MIN, &xUserTask2);
	vTaskSuspend(xUserTask2);
	xTaskCreate(vUserTask3, "T3", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MIN, &xUserTask3);
	vTaskSuspend(xUserTask3);

	xTaskCreate(vTaskGenerator1, "G1", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH , &xTaskGenerator1);
	xTaskCreate(vTaskGenerator2, "G2", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH , &xTaskGenerator2);
	xTaskCreate(vTaskGenerator3, "G3", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH , &xTaskGenerator3);

	xTimerUser1 = xTimerCreate("TMR1", T1_PERIOD, pdTRUE, 0, vCallbackTask1);
	xTimerUser2 = xTimerCreate("TMR2", T2_PERIOD, pdTRUE, 0, vCallbackTask2);
	xTimerUser3 = xTimerCreate("TMR3", T3_PERIOD, pdTRUE, 0, vCallbackTask3);

	xTimerStart(xTimerUser1, 0);
	xTimerStart(xTimerUser2, 0);
	xTimerStart(xTimerUser3, 0);
}

/*-----------------------------------------------------------*/

void vCallbackTask1(TimerHandle_t xTimer)
{
	vTaskResume(xTaskGenerator1);
}

void vCallbackTask2(TimerHandle_t xTimer)
{
	vTaskResume(xTaskGenerator2);
}

void vCallbackTask3(TimerHandle_t xTimer)
{
	vTaskResume(xTaskGenerator3);
}

void vCallbackMonitor(TimerHandle_t xTimer)
{
	vTaskResume(xMonitor);
}

/*-----------------------------------------------------------*/

void Monitor_Task(void* pvParameters)
{
	TickType_t currentTick;

	for (;;)
	{
		currentTick = xTaskGetTickCount();
		printf("\n      \t   Monitor Task: \t\t  %d\n", (int)currentTick);
		dd_task_list** activeTasks = get_active_dd_task_list();
		dd_task_list** completedTasks = get_completed_dd_task_list();
		dd_task_list** overdueTask = get_overdue_dd_task_list();
		vTaskSuspend(NULL);
	}
}

/*-----------------------------------------------------------*/

void vUserTask1(void* pvParameters)
{
	TickType_t currentTick;
	TickType_t prevTick;
	TickType_t executeTick;

	int* pointer = &expectedUser1;

	for (;; )
	{
		currentTick = xTaskGetTickCount();
		executeTick = T1_EXEC;

		while (0 < executeTick)
		{
			prevTick = currentTick;
			currentTick = xTaskGetTickCount();
			if (currentTick != prevTick) {
				executeTick--;
			}
		}
		
		if (*pointer != -1)
		{
			printf("    %d\t   Task 1 complete\t\t  %d\t\t\t  %d\n", ++eventid, (int)currentTick, *pointer);
			pointer++;
		}
		else
			printf("    %d\t   Task 1 complete\t\t  %d\t\t\t  \n", ++eventid, (int)currentTick);
		
		delete_dd_task(taskID1);
	}
}

/*-----------------------------------------------------------*/

void vUserTask2(void* pvParameters)
{
	TickType_t currentTick;
	TickType_t prevTick;
	TickType_t executeTick;
	int* pointer = &expectedUser2;

	for (;; )
	{
		currentTick = xTaskGetTickCount();
		executeTick = T2_EXEC;

		while (0 < executeTick)
		{
			prevTick = currentTick;
			currentTick = xTaskGetTickCount();
			if (currentTick != prevTick) {
				executeTick--;
			}
		}
		
		if (*pointer != -1)
		{
			printf("    %d\t   Task 2 complete\t\t  %d\t\t\t  %d\n", ++eventid, (int)currentTick, *pointer);
			pointer++;
		}
		else
			printf("    %d\t   Task 2 complete\t\t  %d\t\t\t  \n", ++eventid, (int)currentTick);

		delete_dd_task(taskID2);
	}
}

/*-----------------------------------------------------------*/

void vUserTask3(void* pvParameters)
{
	TickType_t currentTick;
	TickType_t prevTick;
	TickType_t executeTick;
	int* pointer = &expectedUser3;

	for (;; )
	{
		currentTick = xTaskGetTickCount();
		executeTick = T3_EXEC;

		while (0 < executeTick)
		{
			prevTick = currentTick;
			currentTick = xTaskGetTickCount();
			if (currentTick != prevTick) {
				executeTick--;
			}
		}

		if (*pointer != -1)
		{
			printf("    %d\t   Task 3 complete\t\t  %d\t\t\t  %d\n", ++eventid, (int)currentTick, *pointer);
			pointer++;
		}
		else
			printf("    %d\t   Task 3 complete\t\t  %d\t\t\t  \n", ++eventid, (int)currentTick);

		delete_dd_task(taskID3);
	}
}

/*-----------------------------------------------------------*/

void vTaskGenerator1(void* pvParameters)
{
	TickType_t currentTick;

	for (;; )
	{
		currentTick = xTaskGetTickCount();

		printf("    %d\t   Task 1 released\t\t  %d\t\t\t  %d\n", ++eventid, (int)currentTick, T1_PERIOD * (taskID1 - 10000));

		if ((int)currentTick >= 1500)
			printf("");

		create_dd_task(xUserTask1, PERIODIC, ++taskID1, T1_PERIOD * (taskID1 - 10000), T1_PERIOD);
	}
}

/*-----------------------------------------------------------*/

void vTaskGenerator2(void* pvParameters)
{
	TickType_t currentTick;

	for (;; )
	{
		currentTick = xTaskGetTickCount();

		printf("    %d\t   Task 2 released\t\t  %d\t\t\t  %d\n", ++eventid, (int)currentTick, T2_PERIOD * (taskID2 - 20000));

		create_dd_task(xUserTask2, PERIODIC, ++taskID2, T2_PERIOD * (taskID2 - 20000), T2_PERIOD);
	}
}

/*-----------------------------------------------------------*/

void vTaskGenerator3(void* pvParameters)
{
	TickType_t currentTick;

	for (;; )
	{
		currentTick = xTaskGetTickCount();

		printf("    %d\t   Task 3 released\t\t  %d\t\t\t  %d\n", ++eventid, (int)currentTick, T3_PERIOD * (taskID3 - 30000));

		if ((int)currentTick > 1500)
		{
			printf("");
		}
		create_dd_task(xUserTask3, PERIODIC, ++taskID3, T3_PERIOD * (taskID3 - 30000), T3_PERIOD);
	}
}

/*-----------------------------------------------------------*/

dd_task allocateEmptyTask()
{
	dd_task emptyTask;

	emptyTask.t_handle = NULL;
	emptyTask.type = PERIODIC;
	emptyTask.task_id = 0;
	emptyTask.absolute_deadline = 0;
	emptyTask.completion_time = 0;
	emptyTask.release_time = 0;

	return emptyTask;
}

void DD_Scheduler(void* pvParameters)
{
	TickType_t currentTick = xTaskGetTickCount();

	dd_task_list* activeTaskList = malloc(sizeof(dd_task_list));
	dd_task_list* completedTaskList = malloc(sizeof(dd_task_list));
	dd_task_list* overdueTaskList = malloc(sizeof(dd_task_list));

	dd_message message_received;
	uint32_t reply;

	activeTaskList->task = allocateEmptyTask();
	completedTaskList->task = allocateEmptyTask();
	overdueTaskList->task = allocateEmptyTask();

	activeTaskList->next_task = NULL;
	completedTaskList->next_task = NULL;
	overdueTaskList->next_task = NULL;

	printf("Event # \t Event \t\t Measured Time (ms) \t Expected Time (ms)\n");
	printf("===========================================================================\n");

	for (;; )
	{
		currentTick = xTaskGetTickCount();

		if (xQueueReceive(xMessageQueue, &message_received, portMAX_DELAY))
		{
			dd_task_list* curr, * prev;
			int flag = 1;

			currentTick = xTaskGetTickCount();

			if (activeTaskList->task.t_handle != NULL) {
				vTaskSuspend(activeTaskList->task.t_handle);
				vTaskPrioritySet(activeTaskList->task.t_handle, PRIORITY_MIN);
			}

			switch (message_received.type)
			{
			case create:
			{
				dd_task_list* node = malloc(sizeof(dd_task_list));

				node->task = message_received.task;
				node->next_task = NULL;

				// Check if schedulable
				if (node->task.absolute_deadline > currentTick + node->task.period)
				{
					curr = overdueTaskList;
					prev = overdueTaskList;

					dd_task_list* node = malloc(sizeof(dd_task_list));
					node->task = activeTaskList->task;
					node->next_task = NULL;

					while (curr->next_task != NULL)
					{
						prev = curr;
						curr = curr->next_task;
					}

					// Add to list at the end
					curr->next_task = node;
				}

				else
				{
					curr = activeTaskList;
					prev = activeTaskList;

					// TRAVERSE LIST
					while (curr->next_task != NULL)
					{
						if (node->task.absolute_deadline < curr->task.absolute_deadline)
						{
							node->next_task = curr;
							if (prev == curr)
							{
								activeTaskList = node;
							}
							else
							{
								prev->next_task = node;
							}
							flag = 0;
							break;
						}
						prev = curr;
						curr = curr->next_task;
					}

					if (flag)
					{
						// EMPTY LIST
						if (curr->task.absolute_deadline == 0)
							activeTaskList = node;

						// ADD ELEMENT TO LEFT
						else if (node->task.absolute_deadline < curr->task.absolute_deadline)
						{
							// ONLY 1
							if (prev == curr)
							{
								node->next_task = curr;
								activeTaskList = node;
							}
							// MANY
							else
							{
								node->next_task = curr;
								prev->next_task = node;
							}
						}
						// ADD ELEMENT TO RIGHT
						else
							curr->next_task = node;
					}
				}

				xQueueSend(xReplyQueue, &reply, portMAX_DELAY);
				break;
			}

			case delete:
			{
				activeTaskList->task.completion_time = currentTick;

				// move to other list
				if (activeTaskList->task.completion_time < activeTaskList->task.absolute_deadline)
				{
					curr = completedTaskList;
					prev = completedTaskList;

					dd_task_list* node = malloc(sizeof(dd_task_list));
					node->task = activeTaskList->task;
					node->next_task = NULL;

					// Traverse list
					while (curr->next_task != NULL)
					{
						prev = curr;
						curr = curr->next_task;
					}

					// Add to list at the end
					curr->next_task = node;
				}

				else
				{
					curr = overdueTaskList;
					prev = overdueTaskList;

					dd_task_list* node = malloc(sizeof(dd_task_list));
					node->task = activeTaskList->task;
					node->next_task = NULL;

					// Traverse list
					while (curr->next_task != NULL)
					{
						prev = curr;
						curr = curr->next_task;
					}

					// Add to list at the end
					curr->next_task = node;
				}

				// DELETE FROM ACTIVE LIST
				curr = activeTaskList;
				prev = activeTaskList;

				// ActiveTaskList 1 element
				if (curr->next_task == NULL)
				{
					activeTaskList->task = allocateEmptyTask();
				}
				else
				{
					activeTaskList->task = curr->next_task->task;
					activeTaskList->next_task = curr->next_task->next_task;
				}
				xQueueSend(xReplyQueue, &reply, portMAX_DELAY);
				break;
			}

			case active:

				xQueueSend(xListQueue, &activeTaskList, portMAX_DELAY);
				vTaskResume(xMonitor);
				break;

			case completed:

				xQueueSend(xListQueue, &completedTaskList, portMAX_DELAY);
				vTaskResume(xMonitor);
				break;

			case overdue:

				xQueueSend(xListQueue, &overdueTaskList, portMAX_DELAY);
				vTaskResume(xMonitor);
				break;

			default:
				break;

			}

			if (activeTaskList->task.absolute_deadline > 0)
			{
				if (activeTaskList->task.release_time == 0)
					activeTaskList->task.release_time = currentTick;

				vTaskPrioritySet(activeTaskList->task.t_handle, PRIORITY_HIGH);

				vTaskResume(activeTaskList->task.t_handle);
			}

		}
	}
}

void create_dd_task(TaskHandle_t t_handle, task_type type, uint32_t task_id, uint32_t absolute_deadline, uint32_t period)
{
	int reply;

	// create the dd_task struct
	dd_task task = {
		t_handle,
		type,
		task_id,
		0, // not yet released
		absolute_deadline,
		0, // not yet completed
		period
	};

	// create a task message to be created
	dd_message message = { create, task };

	// send task message to DD queue
	xQueueSend(xMessageQueue, &message, portMAX_DELAY);

	// wait for reply back from DD scheduler
	xQueueReceive(xReplyQueue, &reply, portMAX_DELAY);

	vTaskSuspend(NULL);

}

/*-----------------------------------------------------------*/

void delete_dd_task(uint32_t task_id)
{
	int reply;

	// create the dd_task struct
	dd_task task = { .task_id = task_id };

	// create a task message to be created
	dd_message message = { delete, task };

	// send task message to DD queue
	xQueueSend(xMessageQueue, &message, portMAX_DELAY);

	// wait for reply back from DD scheduler
	xQueueReceive(xReplyQueue, &reply, portMAX_DELAY);

}

/*-----------------------------------------------------------*/

dd_task_list** get_active_dd_task_list()
{
	dd_task_list* activelist;
	dd_message message = { .type = active };

	xQueueSend(xMessageQueue, &message, portMAX_DELAY);
	xQueueReceive(xListQueue, &activelist, portMAX_DELAY);

	dd_task_list* curr = activelist;
	dd_task_list* prev = activelist;

	int count = 0;

	printf("      \t   Active: \t");
	while (curr->next_task != NULL)
	{
		count++;
		prev = curr;
		curr = curr->next_task;
	}

	if (curr->task.task_id != 0)
	{
		count++;
	}

	printf("%d\n", count);

	return activelist;
}

/*-----------------------------------------------------------*/

dd_task_list** get_completed_dd_task_list()
{
	dd_task_list* completedList;
	dd_message message = { .type = completed };

	xQueueSend(xMessageQueue, &message, portMAX_DELAY);
	xQueueReceive(xListQueue, &completedList, portMAX_DELAY);

	int count = 0;
	int flag = 1;

	dd_task_list* curr = completedList;
	dd_task_list* prev = completedList;

	printf("      \t   Completed: \t");
	while (curr->next_task != NULL)
	{
		if (flag)
		{
			flag = 0;
		}
		else
		{
			count++;
		}
		prev = curr;
		curr = curr->next_task;
	}

	if (curr->task.task_id != 0)
	{
		count++;
	}

	printf("%d\n", count);

	return completedList;
}

/*-----------------------------------------------------------*/

dd_task_list** get_overdue_dd_task_list()
{
	dd_task_list* overdueList;
	dd_message message = { .type = overdue };

	xQueueSend(xMessageQueue, &message, portMAX_DELAY);
	xQueueReceive(xListQueue, &overdueList, portMAX_DELAY);

	int count = 0;
	int flag = 1;

	dd_task_list* curr = overdueList;
	dd_task_list* prev = overdueList;

	printf("      \t   Overdue: \t");
	while (curr->next_task != NULL)
	{
		if (flag)
		{
			flag = 0;
		}
		else
		{
			count++;
		}
		prev = curr;
		curr = curr->next_task;
	}

	if (curr->task.task_id != 0)
	{
		count++;
	}

	printf("%d\n\n", count);

	return overdueList;
}

/*-----------------------------------------------------------*/
