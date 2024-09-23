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
 */

// These values are for the ATtiny13A chip
#define F_CPU 4800000UL
#define EEPSIZE 64
#define V_REF REFS0
#define BOGOMIPS 950

// Here is the NANJG pin layout as needed for the Convoy S2+
#define PWM_PIN     PB1
#define VOLTAGE_PIN PB2
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

#define VOLTAGE_MON         // Comment out to disable LVP

/* This business of talking about a "FET" is a historical artifact
 * from other flashlights.  The Convoy has no FEt, only a set of 7135 chips.
 * Some lights have both a fet (controlled by one PWM pin) and
 * a 7135 (controlled by another pin).
 * So maybe we should change "FET" to "7135" in this source file.
 *
 * What goes on here is that we have a table of PWM settings that
 * we index by 1 ... 7  in the modegroups table.
 * The setting "7" is full on.
 * 0 in the modegroup table is "not in use"
 * (note that we do subtract 1 from the table value)
 */

/* Here is the original set --
 * 0.4, 2.7, 12.5, 25, 42, 50, 100
 */
#define RAMP_SIZE  7
#define RAMP_FET   1,7,32,63,107,127,255

/* The BLF offers 7 levels, sort of as follows:
 * 0.13, 0.5, 5, 17, 24, 43, 100
 *
 * Note that the Convoy cannot achieve the low moonlight
 */

#define TURBO     RAMP_SIZE       // Convenience code for turbo mode

// output to use for blinks on battery check (and other modes)
//#define BLINK_BRIGHTNESS    RAMP_SIZE/4
#define BLINK_BRIGHTNESS    3

// ms per normal-speed blink
#define BLINK_SPEED         (750/4)

#define GROUP_SELECT_MODE 253

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

uint8_t modegroup;     // which mode group (set above in #defines)
uint8_t memory;        // mode memory, or not (set via soldered star)

// Other state variables
uint8_t mode_override; // do we need to enter a special mode?
uint8_t mode_idx;      // current or last-used mode number
uint8_t eepos;

// counter for entering config mode
// (needs to be remembered while off, but only for up to half a second)
uint8_t fast_presses __attribute__ ((section (".noinit")));
uint8_t long_press __attribute__ ((section (".noinit")));

// number of regular non-hidden modes in current mode group
uint8_t solid_modes;

PROGMEM const uint8_t ramp_FET[]  = { RAMP_FET };

// default values calculated by group_calc.py
// Each group must be 8 values long, but can be cut short with a zero.

#ifdef notdef
// #define NUM_MODEGROUPS 12
PROGMEM const uint8_t modegroups[] = {
     1,  2,  3,  5,  7,  POLICE_STROBE, BIKING_STROBE, BATTCHECK,
     1,  2,  3,  5,  7,  0,  0,  0,
     7,  5,  3,  2,  1,  0,  0,  0,
     2,  4,  7,  POLICE_STROBE, BIKING_STROBE, BATTCHECK, SOS,  0,
     2,  4,  7,  0,  0,  0,  0,  0,
     7,  4,  2,  0,  0,  0,  0,  0,
     1,  2,  3,  6,  POLICE_STROBE, BIKING_STROBE, BATTCHECK, SOS,
     1,  2,  3,  6,  0,  0,  0,  0,
     6,  3,  2,  1,  0,  0,  0,  0,
     2,  3,  5,  7,  0,  0,  0,  0,
     7,  4, POLICE_STROBE,0,0,  0,  0,  0,
     7,  0,
};
#endif


/* You might ask why have two levels of lookup here, i.e. why
 * both modegroups and ramp_FET.  Some lights have two PWM channels,
 * so there are two lookup tables (ramp_FET and ramp_7135)
 */
#define NUM_MODEGROUPS 2
PROGMEM const uint8_t modegroups[] = {
     1,  2,  3,  4,  5,  6,  7,  0,
     1,  2,  3,  5,  7,  0,  0,  0,
};

// Modes (gets set when the light starts up based on saved config values)
uint8_t modes[8];  // make sure this is long enough...

/* Here is where we save the value of mode_idx
 * We don't just save it, but we fool around using the
 * entire first half of the EEPROM to perform wear
 * leveling.
 */
#define WEAR_LVL_LEN (EEPSIZE/2)  // must be a power of 2

void
save_mode() {  // save the current mode index (with wear leveling)
    uint8_t oldpos = eepos;

    eepos = (eepos+1) & (WEAR_LVL_LEN-1);  // wear leveling, use next cell

    eeprom_write_byte((uint8_t *)(eepos), mode_idx);  // save current state
    eeprom_write_byte((uint8_t *)(oldpos), 0xff);     // erase old state
}

#define OPT_modegroup (EEPSIZE-1)
#define OPT_memory (EEPSIZE-2)
#define OPT_mode_override (EEPSIZE-3)

void
save_state() {  // central method for writing complete state
	/* tjt - This saves the value of mode_idx */
    save_mode();

    eeprom_write_byte((uint8_t *)OPT_modegroup, modegroup);
    eeprom_write_byte((uint8_t *)OPT_memory, memory);
    eeprom_write_byte((uint8_t *)OPT_mode_override, mode_override);
}

/* tjt - this gets called when we decide this is the first
 * time the light has ever been booted up.
 * It sets these default values, then saves them
 * for the future.
 */
static inline void
reset_state() {
    mode_idx = 0;
	// TJT tjt - I start off in mode group 2
    // modegroup = 1;
    modegroup = 0;
    mode_override = 0;

    save_state();
}

/* tjt - Called once right after startup.
 * It scans the first half of EEPROM looking
 * for a byte that is not 0xff.  If it finds
 * such a byte, that is the selected mode index
 * If not, it calls reset_state()
 *
 * Note that mode_idx is a value 0 .. 7 that picks the
 * mode within the group.
 * modegroup determines what group we are using.
 */
void
restore_state() {
    uint8_t eep;

    uint8_t first = 1;

    // find the mode index data
    for(eepos=0; eepos<WEAR_LVL_LEN; eepos++) {
        eep = eeprom_read_byte((const uint8_t *)eepos);
        if (eep != 0xff) {
            mode_idx = eep;
            first = 0;
            break;
        }
    }

    // if no mode_idx was found, assume this is the first boot
    if (first) {
        reset_state();
        return;
    }

    // load other config values
    modegroup = eeprom_read_byte((uint8_t *)OPT_modegroup);
    memory    = eeprom_read_byte((uint8_t *)OPT_memory);
    mode_override = eeprom_read_byte((uint8_t *)OPT_mode_override);

    // unnecessary, save_state handles wrap-around
    // (and we don't really care about it skipping cell 0 once in a while)
    //else eepos=0;

    if (modegroup >= NUM_MODEGROUPS)
		reset_state();
}

static inline void
next_mode() {
    mode_idx += 1;
    if (mode_idx >= solid_modes) {
        mode_idx = 0;
    }
}

/* tjt - this is called once, early in main()
 * A more apt name would be "setup_modes()" perhaps
 * In particular, based on "modegrup" this copies the 8 values
 * for that selection into the "modes" array.
 *
 * The value of "modegroup" is set in restore_state()
 * which gets called just before this routine gets called.
 */
void
count_modes() {
    /*
     * Determine how many solid modes we have.
     *
     * (this matters because we have more than one set of modes to choose
     *  from, so we need to count at runtime)
     */
    uint8_t *dest = modes;
    const uint8_t *src = modegroups + (modegroup<<3);
    uint8_t count;

    // Figure out how many modes are in this group
    // solid_modes = modegroup + 1;  // Assume group N has N modes
    // No, how about actually counting the modes instead?
    // (in case anyone changes the mode groups above so they don't form a triangle)

    for(count=0; (count<8) && pgm_read_byte(src); count++, src++ ) {
		*dest++ = pgm_read_byte(src);
    }

    solid_modes = count;

}	/* End of count_modes() */

/* Called only by set_level() just below
 */
static inline void
set_output ( uint8_t pwm1 ) {
    /* This is no longer needed since we always use PHASE mode.
    // Need PHASE to properly turn off the light
    if ((pwm1==0) && (pwm2==0)) {
        TCCR0A = PHASE;
    }
    */
    PWM_LVL = pwm1;
}

void
set_level(uint8_t level) {
    TCCR0A = PHASE;
    if (level == 0) {
        set_output(0);
    } else {
        //level -= 1;
        /* apparently not needed on the newer drivers
        if (level == 0) {
            // divide PWM speed by 8 for moon,
            // because the nanjg 105d chips are SLOW
            TCCR0B = 0x02;
        }
        */
        if (level > 2) {
            // divide PWM speed by 2 for moon and low,
            // because the nanjg 105d chips are SLOW
            TCCR0A = FAST;
        }
        set_output ( pgm_read_byte(ramp_FET + level - 1) );
    }
}

// set_mode() could support soft start
#define set_mode set_level

void blink(uint8_t val, uint8_t speed)
{
    for (; val>0; val--)
    {
        set_level(BLINK_BRIGHTNESS);
        _delay_4ms(speed);
        set_level(0);
        _delay_4ms(speed);
        _delay_4ms(speed);
    }
}

/* The "toggle() routine is used to change the setting of
 *  an on/off option.
 * num = 1 for "mode override"
 * num = 2 for "memory"
 * The "mode override" option says that you want the change
 * the mode group for the light.
 * The "memory" option says that you want the light to remember
 *  the last level you used and go back to it when you next
 *  turn the light on.
 *
 * The way this works is that you push 15+ times rapidly and you
 * get the light into a mood to set these two variables.
 * If you set the "mode override" option, the next time you
 * turn the light on, you will enter the group selection
 * algorithm.
 */
void
toggle(uint8_t *var, uint8_t num) {
    // Used for config mode
    // Changes the value of a config option, waits for the user to "save"
    // by turning the light off, then changes the value back in case they
    // didn't save.  Can be used repeatedly on different options, allowing
    // the user to change and save only one at a time.

    blink(num, BLINK_SPEED/4);  // indicate which option number this is

    *var ^= 1;
    save_state();

    // "buzz" for a while to indicate the active toggle window
    blink(32, 500/4/32);

    // if the user didn't click, reset the value and return
    *var ^= 1;
    save_state();
    _delay_s();
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

    // Read config values and saved state
    restore_state();

    // Enable the current mode group
    count_modes();

	/* So, what is a long press?
	 * If the user keeps the light off long enough,
	 * memory fizzles and becomes non-zero.
	 * Just like turning the light on after it has been
	 * off for days or weeks.
	 */

    // check button press time, unless the mode is overridden
    if (! mode_override) {
        if (! long_press) {
			// Aha, a short press (aka a "fast press")
			// detected because this variable is zero

            // Indicates they did a short press, go to the next mode
            // We don't care what the fast_presses value is as long as it's over 15
            fast_presses = (fast_presses+1) & 0x1f;

			// The normal case -- go to the next mode
            next_mode(); // Will handle wrap arounds
        } else {
            // Long press, cancel any count of fast presses.
            fast_presses = 0;

			/* This is the only place the memory option actually does anything.
			 * if it is NOT set, we always start with the first brightness level.
			 */
            if (! memory) {
                // Reset to the first mode
                mode_idx = 0;
            }
        }
    }

    long_press = 0;
    save_mode();

    // Turn features on or off as needed
	// tjt - we DO use this
    #ifdef VOLTAGE_MON
    ADC_on();
    #else
    ADC_off();
    #endif

    uint8_t output;
    uint8_t actual_level;

#ifdef VOLTAGE_MON
    uint8_t lowbatt_cnt = 0;
    uint8_t i = 0;
    uint8_t voltage;
    // Make sure voltage reading is running for later
    ADCSRA |= (1 << ADSC);
#endif

    //output = pgm_read_byte(modes + mode_idx);
    output = modes[mode_idx];
    actual_level = output;

    // handle mode overrides, like mode group selection and temperature calibration
    if (mode_override) {
        // do nothing; mode is already set
        //mode_idx = mode_override;
        fast_presses = 0;
        output = mode_idx;
    }

    while(1) {

		/* Fast presses were getting counted up above.
		 * We get here in all cases, but perhaps the fast_press
		 * count has gotten big enough we should go to Config mode
		 * and let the user toggle the two settings:
		 * memory or mode_override
		 */
        if (fast_presses > 9) {  // Config mode
            _delay_s();       // wait for user to stop fast-pressing button
            fast_presses = 0; // exit this mode after one use
            mode_idx = 0;

            // Enter the mode group selection mode?
            mode_idx = GROUP_SELECT_MODE;
            toggle(&mode_override, 1);
            mode_idx = 0;

            toggle(&memory, 2);

            output = modes[mode_idx];
            actual_level = output;
        }

		/* Here is where you change the group
		 * You only get here by toggling mode_override
		 * in the above once you do the fast presses thing.
		 */
        else if (output == GROUP_SELECT_MODE) {
            // exit this mode after one use
            mode_idx = 0;
            mode_override = 0;

            for(i=0; i<NUM_MODEGROUPS; i++) {
                modegroup = i;
                save_state();

                blink(i+1, BLINK_SPEED/4);

                _delay_s();
				_delay_s();
            }
            _delay_s();
        }

        else {  // Regular non-hidden solid mode
            set_mode(actual_level);
			// Temperature mon stuff used to be here.
            // just sleep.
            _delay_4ms(125);

            // If we got this far, the user has stopped fast-pressing.
            // So, don't enter config mode.
            //fast_presses = 0;
        }

        fast_presses = 0;

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

                if (actual_level > RAMP_SIZE) {  // hidden / blinky modes
                    // step down from blinky modes to medium
                    actual_level = RAMP_SIZE / 2;
                } else if (actual_level > 1) {  // regular solid mode
                    // step down from solid modes somewhat gradually
                    // drop by 25% each time
                    //actual_level = (actual_level >> 2) + (actual_level >> 1);
                    actual_level = actual_level - 1;
                    // drop by 50% each time
                    //actual_level = (actual_level >> 1);
                } else { // Already at the lowest mode
                    //mode_idx = 0;  // unnecessary; we never leave this clause
                    //actual_level = 0;  // unnecessary; we never leave this clause
                    // Turn off the light
                    set_level(0);
                    // Power down as many components as possible
                    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                    sleep_mode();
                }
                set_mode(actual_level);
                output = actual_level;
                //save_mode();  // we didn't actually change the mode
                lowbatt_cnt = 0;
                // Wait before lowering the level again
                //_delay_ms(250);
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
