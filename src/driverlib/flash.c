/*
 * flash.c
 *
 *  Created on: May 14, 2021
 *      Author: Ed Potempa
 */

/*************************/
/* Feature Test Switches */
/*************************/

/******************/
/* System Headers */
/******************/
#include <services/gpio/adi_gpio.h>
#include <drivers/spi/adi_spi.h>

/******************/
/* Local Headers  */
/******************/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "event_groups.h"

#include "main.h"
#include "flash.h"

/**********/
/* Macros */
/**********/
#define flashEVENT_FLASH_SETUP	( 1UL << 0UL )

#define flashSECTOR_ADDR	0x003FF000
#define flashMAX_PAGES		16
#define flashPAGE_SIZE		256
#define flashSTATUS1_BUSY	0x01
#define flashSTATUS1_WEL	0x02

#define flashINSTR_READ_DATA				0x03
#define flashINSTR_WRITE_ENABLE				0x06
#define flashINSTR_READ_STATUS_REGISTER_1	0x05
#define flashINSTR_SECTOR_ERASE				0x20
#define flashINSTR_PAGE_PROGRAM				0x02
#define flashINSTR_WRITE_STATUS_REGISTER	0x01

/************************/
/* File Scope Variables */
/************************/
static uint8_t 			g_aucFlashSpiDriverMem[ADI_SPI_INT_MEMORY_SIZE];
static ADI_SPI_HANDLE	g_xFlashDevHandle;
static uint8_t 			g_aucFlashPageProgram[flashPAGE_SIZE+4];

/*************************/
/* Structures and Unions */
/*************************/
static EventGroupHandle_t 	g_xFlashEvntGrp;

/*************/
/* Functions */
/*************/

/*****	prvFlashSpiCallback  *******************/
static void prvFlashSpiCallback( void *pvHandle, uint32_t ulArg, void *pvArg )
{
TaskHandle_t	xTaskHandle = pvHandle;
ADI_SPI_EVENT 	eEvent = (ADI_SPI_EVENT)ulArg;
uint16_t 		*pusData = (uint16_t *)pvArg;

												/* Which Event? */
	switch( eEvent )
    {
    case ADI_SPI_TRANSCEIVER_PROCESSED:
    	xTaskNotifyFromISR( xTaskHandle, eEvent, eSetValueWithOverwrite, NULL );
    	break;
    default:
        break;
    }
}
/*———————————————————–*/

/*****	xFlashSpiSend  *******************/
static void prvFlashSpiXfer( uint8_t *aucBufOut, BaseType_t xByteOut, uint8_t *aucBufIn, BaseType_t xByteIn )
{
int					i;
ADI_SPI_RESULT		eResult;
uint32_t 			ulEvent;
ADI_SPI_TRANSCEIVER	xTranscever;

											/* Function Arguments checks */
	configASSERT( ( aucBufOut != NULL ) || ( xByteOut != 0 ) )
											/* Select */
	eResult = adi_spi_SlaveSelect( g_xFlashDevHandle, true );
	configASSERT( eResult == ADI_SPI_SUCCESS )
											/* XFER Outs */
	xTranscever.pPrologue = NULL;
	xTranscever.PrologueBytes = 0;
	xTranscever.pTransmitter = aucBufOut;
	xTranscever.TransmitterBytes = xByteOut;
	xTranscever.pReceiver = NULL;
	xTranscever.ReceiverBytes = xByteOut;
	eResult = adi_spi_SubmitBuffer( g_xFlashDevHandle, &xTranscever );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	ulEvent = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
										/* Done if no In bytes */
	if( !xByteIn)
	{
		eResult = adi_spi_SlaveSelect( g_xFlashDevHandle, false );
		configASSERT( eResult == ADI_SPI_SUCCESS )
		return;
	}
										/* Function Arguments checks */
	configASSERT( aucBufIn != NULL )
										/* XFER Ins */
	xTranscever.pPrologue = NULL;
	xTranscever.PrologueBytes = 0;
	xTranscever.pTransmitter = NULL;
	xTranscever.TransmitterBytes = xByteIn;
	xTranscever.pReceiver = aucBufIn;
	xTranscever.ReceiverBytes = xByteIn;
	eResult = adi_spi_SubmitBuffer( g_xFlashDevHandle, &xTranscever );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	ulEvent = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
										/* Not select */
	eResult = adi_spi_SlaveSelect( g_xFlashDevHandle, false );
	configASSERT( eResult == ADI_SPI_SUCCESS )
}
/*———————————————————–*/

/***** prvFlashReadPage ************************/
static void prvFlashReadPage( uint8_t *pucPageBuffer, BaseType_t xPage )
{
uint8_t 	aucTxBuffer[4];
uint32_t	ulAddress;

														/* Function Arguments checks */
	configASSERT( ( xPage > 0 ) && ( xPage <= flashMAX_PAGES ) )
														/* Insert Instruction */
	aucTxBuffer[0] = flashINSTR_READ_DATA;
														/* Insert address */
	ulAddress = ( xPage - 1 ) * flashPAGE_SIZE + flashSECTOR_ADDR;
	aucTxBuffer[1] = (ulAddress >> 16) & 0xff;
	aucTxBuffer[2] = (ulAddress >> 8) & 0xff;
	aucTxBuffer[3] = ulAddress & 0xff;
														/* Send-Receive to w25q32fv */
	prvFlashSpiXfer( aucTxBuffer, 4, pucPageBuffer, flashPAGE_SIZE );
}
/*———————————————————–*/

/***** prvFlashUnprotect ************************/
static void prvFlashUnprotect( void )
{
uint8_t 	aucTxBuffer[3];
uint8_t 	aucRxBuffer[1];

														/* Write Enable */
	aucTxBuffer[0] = flashINSTR_WRITE_ENABLE;
	prvFlashSpiXfer( aucTxBuffer, 1, NULL, 0 );
														/* Write Enable Latch? */
	aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
	prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
	configASSERT( aucRxBuffer[0] & flashSTATUS1_WEL )
														/* Write to Status1 and Status2  */
	aucTxBuffer[0] = flashINSTR_WRITE_STATUS_REGISTER;
	aucTxBuffer[1] = 0x00;
	aucTxBuffer[2] = 0x00;
	prvFlashSpiXfer( aucTxBuffer, 3, NULL, 0 );
														/* Wait till done */
	while( 1 )
	{
		aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
		prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
		if( !( aucRxBuffer[0] & flashSTATUS1_BUSY ) )
			break;
	}
}
/*———————————————————–*/

/***** prvFlashEraseSector ************************/
static void prvFlashEraseSector( void )
{
uint8_t 	aucTxBuffer[4];
uint8_t 	aucRxBuffer[1];

														/* Write Enable */
	aucTxBuffer[0] = flashINSTR_WRITE_ENABLE;
	prvFlashSpiXfer( aucTxBuffer, 1, NULL, 0 );
														/* Write Enable Latch? */
	aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
	prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
	configASSERT( aucRxBuffer[0] & flashSTATUS1_WEL )
														/* Erase Last Sector */
	aucTxBuffer[0] = flashINSTR_SECTOR_ERASE;
	aucTxBuffer[1] = ( flashSECTOR_ADDR >> 16 ) & 0xff;
	aucTxBuffer[2] = ( flashSECTOR_ADDR >> 8 ) & 0xff;
	aucTxBuffer[3] = flashSECTOR_ADDR & 0xff;
	prvFlashSpiXfer( aucTxBuffer, 4, NULL, 0 );
														/* Wait till done */
	while( 1 )
	{
		aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
		prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
		if( !( aucRxBuffer[0] & flashSTATUS1_BUSY ) )
			break;
	}
}
/*———————————————————–*/

/***** prvFlashWritePage ************************/
static void prvFlashWritePage( uint8_t *pucPageBuffer, BaseType_t xPage )
{
int			i;
uint8_t 	aucTxBuffer[4];
uint8_t 	aucRxBuffer[1];
uint32_t	ulAddress;

													/* Function Arguments checks */
	configASSERT( ( xPage > 0 ) && ( xPage <= flashMAX_PAGES ) )
													/* Setup Output buffer */
	g_aucFlashPageProgram[0] = flashINSTR_PAGE_PROGRAM;

	ulAddress = ( xPage - 1 ) * flashPAGE_SIZE + flashSECTOR_ADDR;
	g_aucFlashPageProgram[1] = (ulAddress >> 16) & 0xff;
	g_aucFlashPageProgram[2] = (ulAddress >> 8) & 0xff;
	g_aucFlashPageProgram[3] = ulAddress & 0xff;

	for( i = 0; i < flashPAGE_SIZE; i++ )
	{
		g_aucFlashPageProgram[i+4] = pucPageBuffer[i];
	}
													/* Write Enable */
	aucTxBuffer[0] = flashINSTR_WRITE_ENABLE;
	prvFlashSpiXfer( aucTxBuffer, 1, NULL, 0 );
													/* Write Enable Latch? */
	aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
	prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
	configASSERT( aucRxBuffer[0] & flashSTATUS1_WEL )
													/* Program Page */
	prvFlashSpiXfer( g_aucFlashPageProgram, 260, NULL, 0 );
													/* Wait till done */
	while( 1 )
	{
		aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
		prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
		if( !( aucRxBuffer[0] & flashSTATUS1_BUSY ) )
			break;
	}
}
/*———————————————————–*/

/***** prvFlashProtect ************************/
static void prvFlashProtect( void )
{
uint8_t 	aucTxBuffer[3];
uint8_t 	aucRxBuffer[1];

														/* Write Enable */
	aucTxBuffer[0] = flashINSTR_WRITE_ENABLE;
	prvFlashSpiXfer( aucTxBuffer, 1, NULL, 0 );
														/* Write Enable Latch? */
	aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
	prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
	configASSERT( aucRxBuffer[0] & flashSTATUS1_WEL )
														/* Write to Status1 and Status2  */
	aucTxBuffer[0] = flashINSTR_WRITE_STATUS_REGISTER;
	aucTxBuffer[1] = 0x00;
	aucTxBuffer[2] = 0x40;
	prvFlashSpiXfer( aucTxBuffer, 3, NULL, 0 );
														/* Wait till done */
	while( 1 )
	{
		aucTxBuffer[0] = flashINSTR_READ_STATUS_REGISTER_1;
		prvFlashSpiXfer( aucTxBuffer, 1, aucRxBuffer, 1 );
		if( !( aucRxBuffer[0] & flashSTATUS1_BUSY ) )
			break;
	}
}
/*———————————————————–*/


/***** vFlashIoctl ************************/
void vFlashIoctl( enum FLASH_CMD eCmd, uint8_t *pucPageBuffer, BaseType_t xPage )
{
TaskHandle_t	xTaskHandle;
ADI_SPI_RESULT	eResult;

												/* Function Arguments checks */
	configASSERT( ( eCmd == iFlashReadPage ) || ( eCmd == iFlashWritePage ) ||
			      ( eCmd == iFlashUnprotect ) || ( eCmd == iFlashProtect ) ||
				  ( eCmd == iFlashEraseSector ) )
												/* Wait till Driver setup */
	xEventGroupWaitBits( g_xFlashEvntGrp, flashEVENT_FLASH_SETUP, pdFALSE, pdTRUE, portMAX_DELAY );
												/* Callback */
	xTaskHandle = xTaskGetCurrentTaskHandle( );
	configASSERT( xTaskHandle != NULL )
	eResult = adi_spi_RegisterCallback( g_xFlashDevHandle, prvFlashSpiCallback, xTaskHandle );
	configASSERT( eResult == ADI_SPI_SUCCESS )
												/* Which Command */
	switch( eCmd )
	{
	case iFlashReadPage:
		prvFlashReadPage( pucPageBuffer, xPage );
		break;
	case iFlashUnprotect:
		prvFlashUnprotect( );
		break;
	case iFlashEraseSector:
		prvFlashEraseSector( );
		break;
	case iFlashWritePage:
		prvFlashWritePage( pucPageBuffer, xPage );
		break;
	case iFlashProtect:
		prvFlashProtect( );
		break;
	}
}
/*———————————————————–*/

/*****	prvFlashTask  *******************/
static void prvFlashTask( void *pvParameters )
{
ADI_SPI_RESULT	eResult;

												/* Open the SPI driver */
	eResult = adi_spi_Open( 2, g_aucFlashSpiDriverMem, (uint32_t)ADI_SPI_INT_MEMORY_SIZE, &g_xFlashDevHandle );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetMaster( g_xFlashDevHandle, true );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetWordSize( g_xFlashDevHandle, ADI_SPI_TRANSFER_8BIT );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetClock( g_xFlashDevHandle, 1000 );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetLsbFirst( g_xFlashDevHandle, false );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetClockPolarity( g_xFlashDevHandle, true );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetClockPhase( g_xFlashDevHandle, false );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_EnableDmaMode( g_xFlashDevHandle, false );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetRxWatermark( g_xFlashDevHandle, ADI_SPI_WATERMARK_100, ADI_SPI_WATERMARK_100, ADI_SPI_WATERMARK_100 );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetTxWatermark( g_xFlashDevHandle, ADI_SPI_WATERMARK_0, ADI_SPI_WATERMARK_0, ADI_SPI_WATERMARK_0 );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetSlaveSelect( g_xFlashDevHandle, ADI_SPI_SSEL_ENABLE1 );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_SetHwSlaveSelect( g_xFlashDevHandle, false );
	configASSERT( eResult == ADI_SPI_SUCCESS )
	eResult = adi_spi_ManualSlaveSelect( g_xFlashDevHandle, true );
	configASSERT( eResult == ADI_SPI_SUCCESS )
											/* Signal setup done */
	xEventGroupSetBits( g_xFlashEvntGrp, flashEVENT_FLASH_SETUP );
											/* Done With Initialization - kill Task */
	vTaskDelete( NULL );
}
/*———————————————————–*/

/*****	xFlashInitialize  *******************/
BaseType_t xFlashInitialize( void )
{
int				i;
ADI_SPI_RESULT	eResult;

											/* Create an Event group */
	g_xFlashEvntGrp = xEventGroupCreate();
	if( g_xFlashEvntGrp == NULL )
		return( pdFAIL );
	xEventGroupClearBits( g_xFlashEvntGrp, flashEVENT_FLASH_SETUP );
											/* Create the Flash task */
	if( xTaskCreate( prvFlashTask, (const portCHAR *)"FLASH", configMINIMAL_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + mainPRIORITY_FLASH_TASK, NULL ) != pdTRUE )
		return( pdFAIL );
   											/* Success */
    return( pdPASS );
}
/*———————————————————–*/

