/*
 **
 ** Source file generated on December 14, 2021 at 15:11:53.	
 **
 ** Copyright (C) 2011-2021 Analog Devices Inc., All Rights Reserved.
 **
 ** This file is generated automatically based upon the options selected in 
 ** the Pin Multiplexing configuration editor. Changes to the Pin Multiplexing
 ** configuration should be made by changing the appropriate options rather
 ** than editing this file.
 **
 ** Selected Peripherals
 ** --------------------
 ** SPI2 (CLK, MISO, MOSI, SEL1)
 **
 ** GPIO (unavailable)
 ** ------------------
 ** PB10, PB11, PB12, PB15
 */

#include <sys/platform.h>
#include <stdint.h>

#define SPI2_CLK_PORTB_MUX  ((uint32_t) ((uint32_t) 0<<20))
#define SPI2_MISO_PORTB_MUX  ((uint32_t) ((uint32_t) 0<<22))
#define SPI2_MOSI_PORTB_MUX  ((uint32_t) ((uint32_t) 0<<24))
#define SPI2_SEL1_PORTB_MUX  ((uint32_t) ((uint32_t) 0<<30))

#define SPI2_CLK_PORTB_FER  ((uint32_t) ((uint32_t) 1<<10))
#define SPI2_MISO_PORTB_FER  ((uint32_t) ((uint32_t) 1<<11))
#define SPI2_MOSI_PORTB_FER  ((uint32_t) ((uint32_t) 1<<12))
#define SPI2_SEL1_PORTB_FER  ((uint32_t) ((uint32_t) 1<<15))

int32_t adi_initpinmux(void);

/*
 * Initialize the Port Control MUX and FER Registers
 */
int32_t adi_initpinmux(void) {
    /* PORTx_MUX registers */
    *pREG_PORTB_MUX = SPI2_CLK_PORTB_MUX | SPI2_MISO_PORTB_MUX
     | SPI2_MOSI_PORTB_MUX | SPI2_SEL1_PORTB_MUX;

    /* PORTx_FER registers */
    *pREG_PORTB_FER = SPI2_CLK_PORTB_FER | SPI2_MISO_PORTB_FER
     | SPI2_MOSI_PORTB_FER | SPI2_SEL1_PORTB_FER;
    return 0;
}

