#ifndef TK_ATTINY_H
#define TK_ATTINY_H
/*
 * Attiny portability header.
 * This helps abstract away the differences between various attiny MCUs.
 *
 * Copyright (C) 2015 Selene Scriven
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Choose your MCU here, or in the main .c file, or in the build script
//#define ATTINY 13
//#define ATTINY 25

/******************** hardware-specific values **************************/
#if (ATTINY == 13)
    #define F_CPU 4800000UL
    #define EEPSIZE 64
    #define V_REF REFS0
    #define BOGOMIPS 950

#elif (ATTINY == 25)
    // TODO: Use 6.4 MHz instead of 8 MHz?
    #define F_CPU 8000000UL
    #define EEPSIZE 128
    #define V_REF REFS1
    #define BOGOMIPS (F_CPU/4000)
#else
    Hey, you need to define ATTINY.
#endif


/******************** I/O pin and register layout ************************/

#ifdef NANJG_LAYOUT
#define STAR2_PIN   PB0
#define STAR3_PIN   PB4
#define STAR4_PIN   PB3
#define PWM_PIN     PB1
#define VOLTAGE_PIN PB2
#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

#define PWM_LVL     OCR0B   // OCR0B is the output compare register for PB1

#define FAST 0x23           // fast PWM channel 1 only
#define PHASE 0x21          // phase-correct PWM channel 1 only

#endif  // NANJG_LAYOUT

#ifndef PWM_LVL
    Hey, you need to define an I/O pin layout.
#endif

#endif  // TK_ATTINY_H
