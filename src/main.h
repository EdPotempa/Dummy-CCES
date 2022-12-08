/*
 * main.h
 *
 *  Created on: Feb 1, 2021
 *      Author: Ed Potempa
 */

#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

/**********/
/* Macros */
/**********/
#define	mainVERSION_MAJOR	2
#define	mainVERSION_MINOR	1
#define	mainVERSION_X		2

#define mainPRIORITY_MAIN_TASK		1
#define mainPRIORITY_FLASH_TASK		1
#define mainPRIORITY_LED_TASK		1

#define mainBIT(x,n) (x & (0x01<<n))
#define mainSET(x,n) (x=x | (0x01<<n))
#define mainRES(x,n) (x=x & ~(0x01<<n))

/*************************/
/* Structures and unions */
/*************************/

/************************/
/* External Variables   */
/************************/
extern int g_MainDebug;

/************************/
/* External Functions   */
/************************/


#endif /* SRC_MAIN_H_ */
