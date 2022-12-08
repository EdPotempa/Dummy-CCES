/*
============================================================================
                                       BTECH, INC.
                                       10 Astro Place
                                       Rockaway, NJ 07866

                                       c) Copyright 2021
                                       All Rights Reserved

 Filename:		main.c
 Programmer:	EMP
 Description:
 Revisions:

 Editor -
 Font		    : Consolas
 Font Style     : Regular
 Size           : 12
 Column Marker  : 128
 Tap Stop Value : 4

 See "Coding Standard, Testing and Style Guide" @ www.freertos.org
 Exception to "A space is placed after an opening square bracket, and before a closing
 square bracket"

Loading application into the W25Q32BV Flash:
--------------------------------------------
1)	Create a loader (.LDR) file that can be programmed into a flash device.
	Assuming your project is already loaded into CCES, go to the "Project" menu and choose "Properties", then "C/C++ Build",
	then "Settings", then "Build Artifact" tab.
	There you will see "Artifact Type". From the drop down menu select "Loader File".
	Click on the "Tool Settings" tab and select "CrossCore Blackfin Loader" then "General".
	For "Boot mode" choose "SPIO master" for serial.
	For "Boot format" choose "Intel hex".
	For "Output width" choose "8-bits".
	For "Boot code" enter 1".
	"Initialization file" should not be needed unless part of the example is being loaded to external memory.
	"Use default start address" should be checked.
	Click "OK".
	Rebuild the project to generate the LDR file.
2)	Copy
    "C:\Analog Devices\ADSP-BF706_EZ-KIT_Mini-Rel1.1.0\BF706_EZ-Kit_MINI\Blackfin\Device_programmer\bf706_w25q32bv_dpia.dxe"
     and "S6_VM_XpX.dlr" to "C:\Temp"
3)	Open a Windows Command Prompt and execute "cd C:\Analog Devices\CrossCore Embedded Studio 2.10.0"
4)	Download the LDR file into flash using the CLDP application.
	cldp -proc ADSP-BF706 -emu ICE-1000 -driver "C:\Temp\bf706_w25q32bv_dpia.dxe" -cmd prog -erase affected -format hex -file "C:\Temp\S6_VM_1p0.ldr"


============================================================================
 */

/*******************************************************************************
VERSION 2.0 X1 12/15/21
	- Compiled with CCES 2.10.0
    - Flash Driver Development

*******************************************************************************/

/*************************/
/* Feature Test Switches */
/*************************/

/******************/
/* System Headers */
/******************/
#include <cdefBF706.h>
#include <adi_initialize.h>
#include <services/gpio/adi_gpio.h>
#include <services/pwr/adi_pwr.h>

/******************/
/* Local Headers  */
/******************/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

#include "driverlib/flash.h"
#include "main.h"
#include "led.h"

/**********/
/* Macros */
/**********/

/************************/
/* File Scope Variables */
/************************/
int g_MainDebug;

/*************************/
/* Structures and Unions */
/*************************/

enum VM_TYPE
{
    iVmTypeNoCts = 1,
	iVmTypeDischarge,
	iVmTypeFloat
};
typedef struct VM_CALBRTION
{
	float				fGain;
	float				fOffset;
} tVmCalibration;
struct VM_ID
{
	enum VM_TYPE	eType;
	tVmCalibration	xDc;
	tVmCalibration	xAc;
	tVmCalibration	xTemp[4];
	tVmCalibration	xCt;
	float			fCtSensitivity;
};
union
{
	struct VM_ID	xValues;
	uint8_t			aucBuffer[256];
} g_xVmID;

/*************/
/* Functions */
/*************/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
/* RTOS memory */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer, StackType_t ** ppxIdleTaskStackBuffer,
		                            uint32_t * pulIdleTaskStackSize )
{
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

#if ( configUSE_TIMERS == 1 )
/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer,
		                             uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif /* configUSE_TIMERS == 1 */
#endif /* configSUPPORT_STATIC_ALLOCATION == 1 */

/*****	vApplicationStackOverflowHook *************/
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*———————————————————–*/

/*****	vAssertCalled *************/
void vAssertCalled( const char *pcFile, unsigned long ulLine )
{
volatile uint32_t ul = 0;

    ( void ) pcFile;
    ( void ) ulLine;


    const uint32_t ulMask = cli();

												/* Turn on Error - Turn off Run LED */

	while( ul == 0 )
    {
        NOP();
        NOP();
    }

    sti( ulMask );
}
/*———————————————————–*/

/*****	vApplicationIdleHook *************/
void vApplicationIdleHook( void )
{
	;
}
/*———————————————————–*/

/*****	prvSetupHardware *************/
static void prvSetupHardware( void )
{
int				i;
ADI_GPIO_RESULT eResult1;
ADI_PWR_RESULT	eResult2;
uint32_t		ul;

												/* Initialize PWR service */
	eResult2 = adi_pwr_Init( 0, 25000000);
	configASSERT( eResult2 == ADI_PWR_SUCCESS )
												/* CCLK=400M SYSCLK=200M */
	eResult2 = adi_pwr_SetFreq( 0, 400000000, 200000000 );
	configASSERT( eResult2 == ADI_PWR_SUCCESS )
												/* SCLK0=100M */
	eResult2 = adi_pwr_SetClkDivideRegister( 0, ADI_PWR_CLK_DIV_S0SEL, 2 );
	configASSERT( eResult2 == ADI_PWR_SUCCESS )
												/* SCLK1=100M */
	eResult2 = adi_pwr_SetClkDivideRegister( 0, ADI_PWR_CLK_DIV_S1SEL, 2 );
	configASSERT( eResult2 == ADI_PWR_SUCCESS )
												/* Delay */
	for( i = 0; i < 5000000; i++ )
		;
}
/*———————————————————–*/

/*****	prvMainTask  *******************/
static void prvMainTask( void *pvParameters )
{
int		i;

												/* Off protection */
	vFlashIoctl( iFlashUnprotect, NULL, 0 );
	g_MainDebug++;
												/* Erase */
	vFlashIoctl( iFlashEraseSector, NULL, 0 );
	g_MainDebug++;
												/* Write */
	for( i = 0; i < 256; i ++ )
	{
		g_xVmID.aucBuffer[i] = 0;
	}
	g_xVmID.xValues.eType = iVmTypeDischarge;
	g_xVmID.xValues.xDc.fGain = 1.0;
	g_xVmID.xValues.xDc.fOffset = 0.0;
	g_xVmID.xValues.xAc.fGain = 1.0;
	g_xVmID.xValues.xAc.fOffset = 0.0;
	for( i = 0; i < 4; i++ )
	{
		g_xVmID.xValues.xTemp[i].fGain = 1.0;
		g_xVmID.xValues.xTemp[i].fOffset = 0.0;
	}
	g_xVmID.xValues.fCtSensitivity = .003;
	vFlashIoctl( iFlashWritePage, &g_xVmID.aucBuffer[0], 1 );
	g_MainDebug++;
												/* On protection */
	vFlashIoctl( iFlashProtect, NULL, 0 );
	g_MainDebug++;
												/* Read */
	vFlashIoctl( iFlashReadPage, &g_xVmID.aucBuffer[0], 1 );
	g_MainDebug++;
												/* Done With Initialization - kill Task */
	vTaskDelete( NULL );
 }
/*———————————————————–*/


/*****	xMainInitialize  *******************/
BaseType_t xMainInitialize( void )
{

												/* Create the LED task */
	if( xTaskCreate( prvMainTask, (const portCHAR *)"MAIN", configMINIMAL_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + mainPRIORITY_MAIN_TASK, NULL ) != pdTRUE )
		return( pdFAIL );
    												/* Success */
    return( pdPASS );
}
/*———————————————————–*/

/********/
/* Main */
/********/
int main( int argc, char *argv[] )
{
	/* Initialize managed drivers and/or services */
	adi_initComponents();

	/* Perform any hardware setup necessary. */
	prvSetupHardware();

	/* --- APPLICATION TASKS CAN BE CREATED HERE --- */
	configASSERT( xFlashInitialize() )
	configASSERT( xMainInitialize() )
	configASSERT( xLedInitialize() )

	/* Start the scheduler to start the tasks executing. */
	vTaskStartScheduler();

	/* The following line should never be reached because vTaskStartScheduler()
	will only return if there was not enough FreeRTOS heap memory available to
	create the Idle and (if configured) Timer tasks.  Heap management, and
	techniques for trapping heap exhaustion, are described in the book text. */
	for( ;; );

}

