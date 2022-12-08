/*
 * led.c
 *
 *  Created on: Mar 1, 2021
 *      Author: Ed Potempa
 */


/*************************/
/* Feature Test Switches */
/*************************/

/******************/
/* System Headers */
/******************/
#include <services/gpio/adi_gpio.h>
#include <string.h>

/******************/
/* Local Headers  */
/******************/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "main.h"

/**********/
/* Macros */
/**********/

/************************/
/* File Scope Variables */
/************************/

/*************************/
/* Structures and Unions */
/*************************/

/*************/
/* Functions */
/*************/


/*****	prvLedTask  *******************/
static void prvLedTask( void *pvParameters )
{
ADI_GPIO_RESULT	eResult;
TickType_t 		xLastWakeTime;

												/* Setup Last wake */
	xLastWakeTime = xTaskGetTickCount();
												/* Loop forever */
	while( 1 )
	{
												/* Flip Flop LED */
		eResult = adi_gpio_Toggle( ADI_GPIO_PORT_C, ADI_GPIO_PIN_3 );
		configASSERT( eResult == ADI_GPIO_SUCCESS )
												/* Sleep */
		vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( 1000 ) );
	}
 }
/*———————————————————–*/


/*****	xLedInitialize  *******************/
BaseType_t xLedInitialize( void )
{
ADI_GPIO_RESULT eResult;

												/* Setup LED */
	eResult = adi_gpio_SetDirection( ADI_GPIO_PORT_C, ADI_GPIO_PIN_3, ADI_GPIO_DIRECTION_OUTPUT );
	configASSERT( eResult == ADI_GPIO_SUCCESS )
	eResult = adi_gpio_Set( ADI_GPIO_PORT_C, ADI_GPIO_PIN_3 );
	configASSERT( eResult == ADI_GPIO_SUCCESS )
												/* Create the LED task */
	if( xTaskCreate( prvLedTask, (const portCHAR *)"LED", configMINIMAL_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + mainPRIORITY_LED_TASK, NULL ) != pdTRUE )
		return( pdFAIL );
    												/* Success */
    return( pdPASS );
}
/*———————————————————–*/

