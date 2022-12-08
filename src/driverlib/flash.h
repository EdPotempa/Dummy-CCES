/*
 * flash.h
 *
 *  Created on: May 14, 2021
 *      Author: Ed Potempa
 */

#ifndef SRC_DRIVERLIB_FLASH_H_
#define SRC_DRIVERLIB_FLASH_H_

/**********/
/* Macros */
/**********/

/*************************/
/* Structures and unions */
/*************************/
enum FLASH_CMD {
	iFlashReadPage,
	iFlashUnprotect,
	iFlashEraseSector,
	iFlashWritePage,
	iFlashProtect
};

/************************/
/* External Variables   */
/************************/

/************************/
/* External Functions   */
/************************/
extern BaseType_t	xFlashInitialize( void );
extern void 		vFlashIoctl( enum FLASH_CMD eCmd, uint8_t *pucPageBuffer, BaseType_t xPage );

#endif /* SRC_DRIVERLIB_FLASH_H_ */
