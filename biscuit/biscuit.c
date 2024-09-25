/*
 * "Biscotti" firmware (attiny13a version of "Bistro")
 * This code runs on a single-channel driver with attiny13a MCU.
 * It is intended specifically for nanjg 105d drivers from Convoy.
 *
 * This is "biscuit" -- a severely pruned version of Biscotti,
 *  hacked together by Tom.  I got rid of all blink and strobe
 *  modes, and reduced the 12 groups to just the 1 I wanted.
 *
 * Copyright (C) 2017 Selene Scriven
 * Copyright (C) 2024 Tom Trebisky
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
 *
 * ATTINY13 Diagram for Convoy S2+ (NANJG layout)
 *           ----
 *         -|1  8|- VCC
 *         -|2  7|- Voltage ADC
 *         -|3  6|- PWM (*x7135)
 *     GND -|4  5|-
 *           ----
 *
 * FUSES
 *      I use these fuse settings on attiny13
 *      Low:  0x75
 *      High: 0xff
 *
 * CALIBRATION
 *
 *   To find out what values to use, flash the driver with battcheck.hex
 *   and hook the light up to each voltage you need a value for.  This is
 *   much more reliable than attempting to calculate the values from a
 *   theoretical formula.
 *
 *   Same for off-time capacitor values.  Measure, don't guess.
 */

/* This stuff used to be in tk-attiny.h, but I find
 * it more convenient to have it right here.
 * Many of these definitions are used in the tk-* header files
 */

// These values are for the ATtiny13A chip
#define F_CPU 4800000UL
#define EEPSIZE 64
#define V_REF REFS0
#define BOGOMIPS 950

// Here is the NANJG pin layout as needed for the Convoy S2+
// PWM is on pin 6, pin 7 is the ADC battery monitor
#define PWM_PIN     PB1
// #define VOLTAGE_PIN PB2

#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

// This is the register where we set the PWM level
#define PWM_LVL     OCR0B   // OCR0B is the output compare register for PB1

#define FAST 0x23           // fast PWM channel 1 only
#define PHASE 0x21          // phase-correct PWM channel 1 only

/*
 * =========================================================================
 */

/* We want this to check the battery voltage
 */
#define VOLTAGE_MON

/*
 * =========================================================================
 */

// Ignore a spurious warning, we did the cast on purpose
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <string.h>

#define OWN_DELAY           // Don't use stock delay functions.
#define USE_DELAY_4MS
#define USE_DELAY_S         // Also use _delay_s(), not just _delay_ms()
#include "tk-delay.h"

// This also pulls in tk-calibration.h
#include "tk-voltage.h"

/*
 * global variables
 */

// This variable "decays" to non-zero to indicate long_press
uint8_t long_press __attribute__ ((section (".noinit")));

// current brightness level
/* As an experiment, we give this the same treatment as
 * "long_press" figuring that the value will remain valid
 * after a short press.
 * This seems to work just fine.
 * The original Biscotti kept this in EEPROM with care
 * to do wear leveling as it gets constantly changed.
 */
uint8_t level_idx __attribute__ ((section (".noinit")));

/* The original Biscotti code talked about a FET ramp.
 * This is a historical artifact from other flashlights.
 * The Convoy has no FET, only a set of 7135 chips.
 * Some lights have both a FET (controlled by one PWM pin) and
 * a 7135 (controlled by another pin).
 *
 * In this code I just renamed the whole business "pwm_values"
 * I also did away with the intermediate "modegroups" table
 * since I only have one modegroup.
 *
 * We index the "pwm_values" table with "level_idx" and
 *  that is the end of that.
 * I do put a leading entry of 0 in the table, which turns
 * the light off.  So actual levels are 1 through 7
 * Along with the "0" this gives us 8 levels.
 */

/*  the original set of pwm values gives us these levels:
 * 0, 0.4, 2.7, 12.5, 25, 42, 50, 100
 *
 * The BLF offers 7 levels, sort of as follows:
 * 0, 0.13, 0.5, 5, 17, 24, 43, 100
 *
 * Note that the Convoy cannot achieve the low moonlight
 *  that the BLF-a6 can
 *
 * I decided on this set of levels for the Convoy
 * 0, 0.4, 2.7, 6, 12.5, 25, 50, 100
 */

/* tjt inserts a zero at the start.
 */
// PROGMEM const uint8_t pwm_values[]  = { 0, 1, 7, 32, 63, 107, 127, 255 };
PROGMEM const uint8_t pwm_values[]  = { 0, 1, 7, 15, 32, 63, 127, 255 };

// This includes 0
#define NUM_LEVELS	8

// number of brightness levels (including 0) in the array
uint8_t num_levels = NUM_LEVELS;

/* Note that this wraps around to 1
 * level 0 is off and is used in that way in
 * some parts of this code.
 */
static inline void
next_level ( void )
{
    level_idx += 1;
    if (level_idx >= num_levels) {
        level_idx = 1;
    }
}

/* Call this with a value from 0-7
 *  divide PWM speed by 2 for moon and low,
 *  because the nanjg 105d chips are SLOW
 */
void
set_level ( uint8_t level )
{

    TCCR0A = PHASE;

    if ( level == 0 ) {
		PWM_LVL = 0;
		return;
    }

	if (level > 2)
		TCCR0A = FAST;

	PWM_LVL = pgm_read_byte ( pwm_values + level );
}

int
main(void)
{
    // Assign PWM pin to output
    DDRB |= (1 << PWM_PIN);     // enable main channel

    // Set timer to do PWM for correct output pin and set prescaler timing
    //TCCR0A = 0x23; // phase corrected PWM is 0x21 for PB1, fast-PWM is 0x23
    //TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)
    //TCCR0A = FAST;
    // Set timer to do PWM for correct output pin and set prescaler timing
    TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

	/* So, what is a long press?
	 * If the user keeps the light off long enough,
	 *  memory fizzles and becomes non-zero.
	 * Just like turning the light on after it has been
	 *  off for days or weeks.
	 */

	if ( long_press == 0 && level_idx < num_levels )
		next_level ();
	else
		level_idx = 1;

    long_press = 0;

    // Turn features on or off as needed
	// tjt - we DO use this
    #ifdef VOLTAGE_MON
    ADC_on();
    #else
    ADC_off();
    #endif

#ifdef VOLTAGE_MON
    uint8_t lowbatt_cnt = 0;
    // uint8_t i = 0;
    uint8_t voltage;
    // Make sure voltage reading is running for later
    ADCSRA |= (1 << ADSC);
#endif

    // Regular brightness level
	set_level ( level_idx );

    while(1) {

		// do we need this to pace the voltage monitor?
		_delay_4ms(125);

// tjt - we DO use this
#ifdef VOLTAGE_MON
        if (ADCSRA & (1 << ADIF)) {  // if a voltage reading is ready
            voltage = ADCH;  // get the waiting value

            // See if voltage is lower than what we were looking for
            if (voltage < ADC_LOW) {
                lowbatt_cnt ++;
            } else {
                lowbatt_cnt = 0;
            }

            // See if it's been low for a while, and maybe step down
            if (lowbatt_cnt >= 8) {
                // DEBUG: blink on step-down:
                //set_level(0);  _delay_ms(100);

                if ( level_idx > 1) {  // regular solid mode
                    // step down from solid modes somewhat gradually
                    // drop by 25% each time
                    level_idx = level_idx - 1;
                    // drop by 50% each time
                    // level_idx = (level_idx >> 1);
                } else { // Already at the lowest mode
                    // Turn off the light
					level_idx = 0;
                    set_level ( 0 );
                    // Power down as many components as possible
                    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                    sleep_mode();
					/* NOTREACHED */
                }

                set_level ( level_idx );

                lowbatt_cnt = 0;
                // Wait before lowering the level again
                _delay_s();
            }

            // Make sure conversion is running for next time through
            ADCSRA |= (1 << ADSC);
        }
#endif  // ifdef VOLTAGE_MON

    } /* end of forever loop */

    //return 0; // Standard Return Code
}	/* end of main () */

/* THE END */
