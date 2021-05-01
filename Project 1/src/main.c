// Kutay Cinar
// V00******

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx_gpio.h"

/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"

/*-----------------------------------------------------------*/

#define FLOW_MIN	40
#define FLOW_MAX	4000

#define TICK_RATE	pdMS_TO_TICKS(100)

#define RED			GPIO_Pin_0
#define YELLOW		GPIO_Pin_1
#define GREEN		GPIO_Pin_2

#define POT			GPIO_Pin_3

#define DATA		GPIO_Pin_6
#define CLOCK		GPIO_Pin_7
#define RESET		GPIO_Pin_8

#define YES			1
#define NO			0

/*-----------------------------------------------------------*/

void Traffic_Flow_Adjustment_Task	( void *pvParameters );
void Traffic_Light_State_Task		( void *pvParameters );
void Traffic_Generator_Task			( void *pvParameters );
void System_Display_Task			( void *pvParameters );

void vCallbackGreen 	( TimerHandle_t xTimer );
void vCallbackYellow 	( TimerHandle_t xTimer );
void vCallbackRed 		( TimerHandle_t xTimer );

void myGPIOC_Init	(void);
void myADC_Init		(void);

uint16_t getADCValue(void);

xQueueHandle xQueue_FLOW = 0;
xQueueHandle xQueue_LIGHT = 0;
xQueueHandle xQueue_GENERATOR = 0;

TimerHandle_t xTimer_GREEN 	= 0;
TimerHandle_t xTimer_YELLOW = 0;
TimerHandle_t xTimer_RED 	= 0;

/*-----------------------------------------------------------*/

int main(void)
{
	// Initialization functions
	myGPIOC_Init();
	myADC_Init();

	// Create the queue
	xQueue_FLOW  = xQueueCreate( 1, sizeof(int) );
	xQueue_LIGHT = xQueueCreate( 1, sizeof(uint16_t) );
	xQueue_GENERATOR = xQueueCreate( 1, sizeof(int) );

	if( xQueue_FLOW != NULL	&& xQueue_LIGHT != NULL && xQueue_GENERATOR != NULL)
	{
		xTaskCreate( Traffic_Flow_Adjustment_Task, 	"Traffic_Flow_Adjustment", 	configMINIMAL_STACK_SIZE, NULL, 1, NULL);
		xTaskCreate( Traffic_Light_State_Task, 		"Traffic_Light_State", 		configMINIMAL_STACK_SIZE, NULL, 1, NULL);
		xTaskCreate( Traffic_Generator_Task, 		"Traffic_Generator", 		configMINIMAL_STACK_SIZE, NULL, 1, NULL);
		xTaskCreate( System_Display_Task, 			"System_Display",	 		configMINIMAL_STACK_SIZE, NULL, 1, NULL);

		xTimer_GREEN 	 =	xTimerCreate("TMR1", pdMS_TO_TICKS(5000), pdFALSE, 0, vCallbackGreen);
		xTimer_YELLOW 	 =	xTimerCreate("TMR2", pdMS_TO_TICKS(3000), pdFALSE, 0, vCallbackYellow);
		xTimer_RED		 =	xTimerCreate("TMR3", pdMS_TO_TICKS(5000), pdFALSE, 0, vCallbackRed);

		/* Start the tasks and timer running. */
		vTaskStartScheduler();

	}

	for( ;; );

}

/*-----------------------------------------------------------*/

void myGPIOC_Init()
{
	/* Enable clock for GPIO C */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	/* Initialize GPIO Structure */
	GPIO_InitTypeDef GPIO_InitStruct;

	/* Enable PC0-PC2 Output */
	GPIO_InitStruct.GPIO_Pin = 					RED | YELLOW | GREEN;
	GPIO_InitStruct.GPIO_Mode = 				GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed =				GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* Enable PC3 Analog */
	GPIO_InitStruct.GPIO_Pin =					POT;
	GPIO_InitStruct.GPIO_Mode =					GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_PuPd =					GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* Enable PC6-PC8 Output */
	GPIO_InitStruct.GPIO_Pin = 					DATA | CLOCK | RESET;
	GPIO_InitStruct.GPIO_Mode = 				GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed =				GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/*-----------------------------------------------------------*/

void myADC_Init()
{
	/* Enable clock for ADC */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* Initialize ADC Structure */
	ADC_InitTypeDef ADC_InitStruct;

	/* ADC1 Structure */
	ADC_InitStruct.ADC_ContinuousConvMode = 	DISABLE;
	ADC_InitStruct.ADC_DataAlign = 				ADC_DataAlign_Right;
	ADC_InitStruct.ADC_Resolution = 			ADC_Resolution_12b;
	ADC_InitStruct.ADC_ScanConvMode =			DISABLE;
	ADC_InitStruct.ADC_ExternalTrigConv =		DISABLE;
	ADC_InitStruct.ADC_ExternalTrigConvEdge = 	DISABLE;

	/* Apply initialization */
	ADC_Init(ADC1, &ADC_InitStruct);

	/* Enable ADC */
	ADC_Cmd(ADC1, ENABLE);

	/* ADC Channel Config */
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 1, ADC_SampleTime_144Cycles);
}

/*-----------------------------------------------------------*/

uint16_t getADCValue(void)
{
	ADC_SoftwareStartConv(ADC1);

	/* Checking for EoC (End of Conversion) flag */
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));

	return ADC_GetConversionValue(ADC1);
}

/*-----------------------------------------------------------*/

void Traffic_Flow_Adjustment_Task( void *pvParameters )
{
	int pot_value;

	for( ;; )
	{
		// Obtain potentiometer value and scale it to a 0-100 range.
		pot_value = ( 100 * ( (int) getADCValue() - FLOW_MIN)  / (FLOW_MAX - FLOW_MIN) );

		// Overwrite existing flow in queue with new potentiometer value
		if( xQueueOverwrite(xQueue_FLOW, &pot_value) )
		{
			vTaskDelay( TICK_RATE );
		}
	}
}

/*-----------------------------------------------------------*/

void Traffic_Light_State_Task( void *pvParameters )
{
	xTimerStart( xTimer_GREEN, 0);

	uint16_t LIGHT = GREEN;
	xQueueOverwrite(xQueue_LIGHT, &LIGHT);

	for( ;; )
	{
		vTaskDelay(100 * TICK_RATE );
	}

}

/*-----------------------------------------------------------*/

void vCallbackGreen ( TimerHandle_t xTimer)
{
	xTimerStart( xTimer_YELLOW, 0 );

	uint16_t LIGHT = YELLOW;
	xQueueOverwrite(xQueue_LIGHT, &LIGHT);
}

/*-----------------------------------------------------------*/

void vCallbackYellow ( TimerHandle_t xTimer)
{
	int flow;
	BaseType_t xStatus = xQueuePeek( xQueue_FLOW, &flow, TICK_RATE );

	if ( xStatus == pdPASS )
	{
		xTimerChangePeriod( xTimer_RED, pdMS_TO_TICKS(10000 - 50 * flow), 0 );

		uint16_t LIGHT = RED;
		xQueueOverwrite(xQueue_LIGHT, &LIGHT);
	}
}

/*-----------------------------------------------------------*/

void vCallbackRed ( TimerHandle_t xTimer)
{
	int flow;
	BaseType_t xStatus = xQueuePeek( xQueue_FLOW, &flow, TICK_RATE );

	if ( xStatus == pdPASS )
	{
		xTimerChangePeriod( xTimer_GREEN, pdMS_TO_TICKS(5000 + 50 * flow), 0 );

		uint16_t LIGHT = GREEN;
		xQueueOverwrite(xQueue_LIGHT, &LIGHT);
	}
}

/*-----------------------------------------------------------*/

void Traffic_Generator_Task( void *pvParameters )
{
	BaseType_t xStatus;

	int flow, car = YES;

	for ( ;; )
	{
		xStatus = xQueuePeek( xQueue_FLOW, &flow, TICK_RATE );

		if ( xStatus == pdPASS )
		{
			xQueueOverwrite(xQueue_GENERATOR, &car);

			vTaskDelay( pdMS_TO_TICKS(4000 - 35 * flow) );
		}
	}

}

/*-----------------------------------------------------------*/

void System_Display_Task( void *pvParameters )
{
	BaseType_t xStatus_LIGHT, xStatus_GENERATOR;

	uint16_t LIGHT;

	int car;
	int cars[20] = {NO};

	GPIO_SetBits(GPIOC, RESET);

	for( ;; )
	{

		// Traffic Lights LED pins
		xStatus_LIGHT = xQueuePeek( xQueue_LIGHT, &LIGHT, TICK_RATE );

		if ( xStatus_LIGHT == pdTRUE )
		{
			GPIO_ResetBits(GPIOC, RED);
			GPIO_ResetBits(GPIOC, YELLOW);
			GPIO_ResetBits(GPIOC, GREEN);

			GPIO_SetBits(GPIOC, LIGHT);
		}

		// Traffic Car LEDS shift register
		for (int i = 20; i>0; i--)
		{
			if ( cars[i] == YES )
				GPIO_SetBits(GPIOC, DATA);
			else
				GPIO_ResetBits(GPIOC, DATA);

			// Cycle
			GPIO_SetBits(GPIOC, CLOCK);
			GPIO_ResetBits(GPIOC, CLOCK);
		}

		// Moving ALL cars on GREEN lights
		if ( LIGHT == GREEN )
		{
			for (int i = 19; i > 0; i--)
				cars[i] = cars[i-1];
		}

		// Conditions for stopping on RED and YELLOW lights
		else
		{
			// Shifting cars before stop sign
			for (int i = 8; i > 0; i--)
			{
				if (cars[i] == NO)
				{
					cars[i] = cars[i-1];
					cars[i-1] = NO;
				}
			}

			// Shifting cars after stop sign
			for (int i = 19; i > 9; i--)
			{
				cars[i] = cars[i-1];
				cars[i-1] = NO;
			}
		}

		// Check the Traffic Generator Queue to see if a new car has arrived
		xStatus_GENERATOR = xQueueReceive( xQueue_GENERATOR, &car, pdMS_TO_TICKS(100) );

		cars[0] = NO; // no car by default

		if ( xStatus_GENERATOR == pdTRUE )
		{
			if ( car == YES)
				cars[0] = YES; // add car if arrived
		}

		vTaskDelay( 5 * TICK_RATE );

	}

}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

/*-----------------------------------------------------------*/

