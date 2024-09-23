/*
 * "Biscotti" firmware (attiny13a version of "Bistro")
 * This code runs on a single-channel driver with attiny13a MCU.
 * It is intended specifically for nanjg 105d drivers from Convoy.
 *
 * Copyright (C) 2017 Selene Scriven
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

// Choose your MCU here, or in the build script
#define ATTINY 13
//#define ATTINY 25

#define NANJG_LAYOUT  // specify an I/O pin layout
#include "tk-attiny.h"

/*
 * =========================================================================
 * Settings to modify per driver
 */

#define VOLTAGE_MON         // Comment out to disable LVP

// ../../bin/level_calc.py 64 1 10 1300 y 3 0.23 140
// level_calc.py 1 4 7135 9 8 700
// level_calc.py 1 3 7135 9 8 700

#define RAMP_SIZE  7
//#define RAMP_FET   6,12,34,108,255
#define RAMP_FET   1,7,32,63,107,127,255

// Enable battery indicator mode?
#define USE_BATTCHECK

// Choose a battery indicator style
#define BATTCHECK_4bars  // up to 4 blinks
//#define BATTCHECK_8bars  // up to 8 blinks
//#define BATTCHECK_VpT  // Volts + tenths

// output to use for blinks on battery check (and other modes)
//#define BLINK_BRIGHTNESS    RAMP_SIZE/4
#define BLINK_BRIGHTNESS    3

// ms per normal-speed blink
#define BLINK_SPEED         (750/4)

// Hidden modes are *before* the lowest (moon) mode, and should be specified
// in reverse order.  So, to go backward from moon to turbo to strobe to
// battcheck, use BATTCHECK,STROBE,TURBO .
//#define HIDDENMODES         BATTCHECK,STROBE,TURBO

#define TURBO     RAMP_SIZE       // Convenience code for turbo mode
#define BATTCHECK 254       // Convenience code for battery check mode
#define GROUP_SELECT_MODE 253

// Uncomment to enable tactical strobe mode
// TJT comments this out to save space.
// #define ANY_STROBE  // required for strobe or police_strobe
//#define STROBE    251       // Convenience code for strobe mode

// Uncomment to unable a 2-level stutter beacon instead of a tactical strobe
#define BIKING_STROBE 250   // Convenience code for biking strobe mode
// comment out to use minimal version instead (smaller)
// TJT comments this out to save space.
//#define FULL_BIKING_STROBE
//#define RAMP 249       // ramp test mode for tweaking ramp shape

#define POLICE_STROBE 248
//#define RANDOM_STROBE 247

#define SOS 246

// Calibrate voltage and OTC in this file:
#include "tk-calibration.h"

/*
 * =========================================================================
 */

// Ignore a spurious warning, we did the cast on purpose
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#include <avr/pgmspace.h>
//#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
//#include <avr/power.h>
#include <string.h>

#define OWN_DELAY           // Don't use stock delay functions.
#define USE_DELAY_4MS
#define USE_DELAY_S         // Also use _delay_s(), not just _delay_ms()
#include "tk-delay.h"

#include "tk-voltage.h"

#ifdef RANDOM_STROBE
#include "tk-random.h"
#endif

/*
 * global variables
 */

uint8_t modegroup;     // which mode group (set above in #defines)

#define enable_moon 0   // Should we add moon to the set of modes?
#define reverse_modes 0 // flip the mode order?

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

// number of hidden modes in the current mode group
// (hardcoded because both groups have the same hidden modes)
//uint8_t hidden_modes = NUM_HIDDEN;  // this is never used

//PROGMEM const uint8_t hiddenmodes[] = { HIDDENMODES };

// default values calculated by group_calc.py
// Each group must be 8 values long, but can be cut short with a zero.

#define NUM_MODEGROUPS 12

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

uint8_t modes[8];  // make sure this is long enough...

// Modes (gets set when the light starts up based on saved config values)
//PROGMEM const uint8_t ramp_7135[] = { RAMP_7135 };
PROGMEM const uint8_t ramp_FET[]  = { RAMP_FET };

/* Here is where we save the value of mode_idx
 * We don't just save it, but we fool around using the
 * entire first half of the EEPROM to perform wear
 * leveling.
 */
#define WEAR_LVL_LEN (EEPSIZE/2)  // must be a power of 2

void
save_mode() {  // save the current mode index (with wear leveling)
    uint8_t oldpos=eepos;

    eepos = (eepos+1) & (WEAR_LVL_LEN-1);  // wear leveling, use next cell
    /*
    eepos ++;
    if (eepos > (EEPSIZE-4)) {
        eepos = 0;
    }
    */

    eeprom_write_byte((uint8_t *)(eepos), mode_idx);  // save current state
    eeprom_write_byte((uint8_t *)(oldpos), 0xff);     // erase old state
}

//#define OPT_firstboot (EEPSIZE-1)
#define OPT_modegroup (EEPSIZE-1)
#define OPT_memory (EEPSIZE-2)
//#define OPT_offtim3 (EEPSIZE-4) -- not used
//#define OPT_maxtemp (EEPSIZE-5) -- not used
#define OPT_mode_override (EEPSIZE-3)
//#define OPT_moon (EEPSIZE-7)
//#define OPT_revmodes (EEPSIZE-8)
//#define OPT_muggle (EEPSIZE-9) -- not used

void
save_state() {  // central method for writing complete state
	/* tjt - This saves the value of mode_idx */
    save_mode();

    eeprom_write_byte((uint8_t *)OPT_modegroup, modegroup);
    eeprom_write_byte((uint8_t *)OPT_memory, memory);
    eeprom_write_byte((uint8_t *)OPT_mode_override, mode_override);
    //eeprom_write_byte((uint8_t *)OPT_moon, enable_moon);
    //eeprom_write_byte((uint8_t *)OPT_revmodes, reverse_modes);
    //eeprom_write_byte((uint8_t *)OPT_muggle, muggle_mode);
}

/* tjt - this gets called when we decide this is the first
 * time the light has ever been booted up.
 * It sets these default values, then saves them
 * for the future.
 */
static inline void reset_state() {
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
    //enable_moon   = eeprom_read_byte((uint8_t *)OPT_moon);
    //reverse_modes = eeprom_read_byte((uint8_t *)OPT_revmodes);
    //muggle_mode   = eeprom_read_byte((uint8_t *)OPT_muggle);

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
        // Wrap around, skipping the hidden modes
        // (note: this also applies when going "forward" from any hidden mode)
        // FIXME? Allow this to cycle through hidden modes?
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
     * Determine how many solid and hidden modes we have.
     *
     * (this matters because we have more than one set of modes to choose
     *  from, so we need to count at runtime)
     */
    uint8_t *dest;
    //const uint8_t *src = modegroups + (my_modegroup<<3);
    const uint8_t *src = modegroups + (modegroup<<3);
    dest = modes;

    // Figure out how many modes are in this group
    // solid_modes = modegroup + 1;  // Assume group N has N modes
    // No, how about actually counting the modes instead?
    // (in case anyone changes the mode groups above so they don't form a triangle)
    uint8_t count;

    for(count=0;
        (count<8) && pgm_read_byte(src);
        count++, src++ )
    {
        *dest++ = pgm_read_byte(src);
    }
    solid_modes = count;

}	/* End of count_modes() */

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

#ifdef ANY_STROBE
static inline void strobe(uint8_t ontime, uint8_t offtime) {
    uint8_t i;
    for(i=0;i<8;i++) {
        set_level(RAMP_SIZE);
        _delay_4ms(ontime);
        set_level(0);
        _delay_4ms(offtime);
    }
}
#endif

#ifdef SOS
static inline void SOS_mode() {
#define SOS_SPEED (200/4)
    blink(3, SOS_SPEED);
    _delay_4ms(SOS_SPEED*5);
    blink(3, SOS_SPEED*5/2);
    //_delay_4ms(SOS_SPEED);
    blink(3, SOS_SPEED);
    _delay_s(); _delay_s();
}
#endif

void toggle(uint8_t *var, uint8_t num) {
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
    /*
    for(uint8_t i=0; i<32; i++) {
        set_level(BLINK_BRIGHTNESS * 3 / 4);
        _delay_4ms(30);
        set_level(0);
        _delay_4ms(30);
    }
    */
    // if the user didn't click, reset the value and return
    *var ^= 1;
    save_state();
    _delay_s();
}

int main(void)
{
    // check the OTC immediately before it has a chance to charge or discharge

    // Set PWM pin to output
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


    // TODO: Enable this?  (might prevent some corner cases, but requires extra room)
    // memory decayed, reset it
    // (should happen on med/long press instead
    //  because mem decay is *much* slower when the OTC is charged
    //  so let's not wait until it decays to reset it)
    //if (fast_presses > 0x20) { fast_presses = 0; }

    // check button press time, unless the mode is overridden
    if (! mode_override) {
        if (! long_press) {
            // Indicates they did a short press, go to the next mode
            // We don't care what the fast_presses value is as long as it's over 15
            fast_presses = (fast_presses+1) & 0x1f;
            next_mode(); // Will handle wrap arounds
        } else {
            // Long press, keep the same mode
            // ... or reset to the first mode
            fast_presses = 0;
            if (! memory) {
                // Reset to the first mode
                mode_idx = 0;
            }
        }
    }
    long_press = 0;
    save_mode();

    #ifdef CAP_PIN
    // Charge up the capacitor by setting CAP_PIN to output
    DDRB  |= (1 << CAP_PIN);    // Output
    PORTB |= (1 << CAP_PIN);    // High
    #endif

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
        if (fast_presses > 9) {  // Config mode
            _delay_s();       // wait for user to stop fast-pressing button
            fast_presses = 0; // exit this mode after one use
            mode_idx = 0;

            //toggle(&memory, 2);

            //toggle(&enable_moon, 3);

            //toggle(&reverse_modes, 4);

            // Enter the mode group selection mode?
            mode_idx = GROUP_SELECT_MODE;
            toggle(&mode_override, 1);
            mode_idx = 0;

            toggle(&memory, 2);

            //toggle(&firstboot, 8);

            //output = pgm_read_byte(modes + mode_idx);
            output = modes[mode_idx];
            actual_level = output;
        }
#ifdef STROBE
        else if (output == STROBE) {
            // 10Hz tactical strobe
            strobe(33/4,67/4);
        }
#endif // ifdef STROBE

// TJT wraps this in ANY_STROBE
#ifdef ANY_STROBE
#ifdef POLICE_STROBE
        else if (output == POLICE_STROBE) {
            // police-like strobe
            //for(i=0;i<8;i++) {
                strobe(20/4,40/4);
            //}
            //for(i=0;i<8;i++) {
                strobe(40/4,80/4);
            //}
        }
#endif // ifdef POLICE_STROBE
#endif // ifdef ANY_STROBE

#ifdef RANDOM_STROBE
        else if (output == RANDOM_STROBE) {
            // pseudo-random strobe
            uint8_t ms = (34 + (pgm_rand() & 0x3f))>>2;
            strobe(ms, ms);
            //strobe(ms, ms);
        }
#endif // ifdef RANDOM_STROBE

#ifdef BIKING_STROBE
        else if (output == BIKING_STROBE) {
            // 2-level stutter beacon for biking and such
#ifdef FULL_BIKING_STROBE
            // normal version
            for(i=0;i<4;i++) {
                set_mode(RAMP_SIZE);
                _delay_4ms(3);
                set_mode(4);
                _delay_4ms(15);
            }
            //_delay_ms(720);
            _delay_s();
#else
            // small/minimal version
            set_mode(RAMP_SIZE);
            _delay_4ms(8);
            set_mode(3);
            _delay_s();
#endif
        }
#endif  // ifdef BIKING_STROBE
#ifdef SOS
        else if (output == SOS) { SOS_mode(); }
#endif // ifdef SOS

#ifdef RAMP
        else if (output == RAMP) {
            int8_t r;
            // simple ramping test
            for(r=1; r<=RAMP_SIZE; r++) {
                set_level(r);
                _delay_4ms(6);
            }
            for(r=RAMP_SIZE; r>0; r--) {
                set_level(r);
                _delay_4ms(6);
            }
        }
#endif  // ifdef RAMP

#ifdef BATTCHECK
        else if (output == BATTCHECK) {
#ifdef BATTCHECK_VpT
            // blink out volts and tenths
            _delay_4ms(25);
            uint8_t result = battcheck();
            blink(result >> 5, BLINK_SPEED/8);
            _delay_4ms(BLINK_SPEED);
            blink(1,5/4);
            _delay_4ms(254);
            blink(result & 0b00011111, BLINK_SPEED/8);
#else  // ifdef BATTCHECK_VpT
            // blink zero to five times to show voltage
            // (~0%, ~25%, ~50%, ~75%, ~100%, >100%)
            blink(battcheck(), BLINK_SPEED/4);
#endif  // ifdef BATTCHECK_VpT
            // wait between readouts
            _delay_s(); _delay_s();
        }
#endif // ifdef BATTCHECK

        else if (output == GROUP_SELECT_MODE) {
            // exit this mode after one use
            mode_idx = 0;
            mode_override = 0;

            for(i=0; i<NUM_MODEGROUPS; i++) {
                modegroup = i;
                save_state();

                blink(i+1, BLINK_SPEED/4);
                _delay_s(); _delay_s();
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
