/*

 ---------------------------------------------------------------------

  This version has been heavily stripped down for code clarity,
  mostly by removing features by the shovel load.
      ---Calvin Li, April 2015

 ---------------------------------------------------------------------


 K3NG Arduino CW Keyer

 Copyright 1340 BC, 2010, 2011, 2012, 2013, 2014 Anthony Good, K3NG
 All trademarks referred to in source code and documentation are copyright their respective owners.

    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    

If you offer a hardware kit using this software, show your appreciation by sending the author a complimentary kit or a bottle of bourbon ;-)

Full documentation can be found at http://blog.radioartisan.com/arduino-cw-keyer/ .  Please read it before requesting help.

 Command Line Interface ("CLI") (USB Port) (Note: turn on carriage return if using Arduino Serial Monitor program)

    CW Keyboard: type what you want the keyer to send (all commands are preceded with a backslash ( \ )
    \?     Help                                      (requires FEATURE_SERIAL_HELP)
    \#     Play memory #                             (requires FEATURES_MEMORIES; play memories 1 - 10 (0 = memory 10) )
    \a     Iambic A mode
    \b     Iambic B mode
    \c     Switch to CW (from Hell)
    \d     Ultimatic mode
    \e#### Set serial number to ####
    \f#### Set sidetone frequency to #### hertz
    \g     Bug mode
    \h     Switch to Hell sending                    (requires FEATURE_HELL)
    \i     Transmit enable/disable
    \j###  Dah to dit ratio (300 = 3.00)
    \k     Callsign receive practice
    \l##   Set weighting (50 = normal)
    \m###  Set Farnsworth speed
    \n     Toggle paddle reverse
    \o     Toggle sidetone on/off
    \p#    Program memory #  ( 1 - 9, 0 = memory 10)
    \q##   Switch to QRSS mode, dit length ## seconds
    \r     Switch to regular speed mode
    \s     Status
    \t     Tune mode
    \u     Manual PTT toggle
    \v     Toggle potentiometer active / inactive   (requires FEATURE_POTENTIOMETER)
    \w###  Set speed in WPM
    \x#    Switch to transmitter #
    \y#    Change wordspace to # elements (# = 1 to 9)
    \z     Autospace on/off
    \+     Create prosign
    \!##   Repeat play memory
    \|#### Set memory repeat (milliseconds)
    \&     Toggle CMOS Super Keyer Timing on/off
    \%##   Set CMOS Super Keyer Timing %     
    \.     Toggle dit buffer on/off
    \-     Toggle dah buffer on/off
    \~     Reset unit
    \:     Toggle cw send echo
    \{     QLF mode on/off

 
 Useful Stuff
    Reset to defaults: squeeze both paddles at power up (good to use if you dorked up the speed and don't have the CLI)
    Press the right paddle to enter straight key mode at power up
    Press the leftpaddle at power up to enter and stay forever in beacon mode

*/

#define CODE_VERSION "2.2.2015040401"
#define eeprom_magic_number 19

#include <stdio.h>

#ifndef HARDWARE_ARDUINO_DUE
 #include <EEPROM.h>
#else
 #include <SPI.h>
#endif //HARDWARE_ARDUINO_DUE

#include <avr/pgmspace.h>

#ifndef HARDWARE_ARDUINO_DUE
 #include <avr/wdt.h>
#endif //HARDWARE_ARDUINO_DUE


// necessary for arduino-mk; remove for IDE
#include "keyer.h"

#include "keyer_features_and_options.h"
#include "keyer_debug.h"
#include "keyer_pin_settings.h"
#include "keyer_settings.h"

#if defined(FEATURE_SLEEP)
 #include <avr/sleep.h>
#endif 

#if defined(FEATURE_COMMAND_LINE_INTERFACE)
 #define FEATURE_SERIAL
#endif


// Variables and stuff
struct config_t {  // 23 bytes
  unsigned int wpm;
  byte paddle_mode;
  byte keyer_mode;
  byte sidetone_mode;
  unsigned int hz_sidetone;
  unsigned int dah_to_dit_ratio;
  byte pot_activated;
  byte length_wordspace;
  byte autospace_active;
  unsigned int wpm_farnsworth;
  byte current_ptt_line;
  byte current_tx;
  byte weighting;
  unsigned int memory_repeat_time;
  byte dit_buffer_off;
  byte dah_buffer_off;
  byte cmos_super_keyer_iambic_b_timing_percent;
  byte cmos_super_keyer_iambic_b_timing_on;
} configuration;


byte command_mode_disable_tx = 0;
byte current_tx_key_line = tx_key_line_1;
unsigned int ptt_tail_time[] = {initial_ptt_tail_time_tx1,initial_ptt_tail_time_tx2,initial_ptt_tail_time_tx3,initial_ptt_tail_time_tx4,initial_ptt_tail_time_tx5,initial_ptt_tail_time_tx6};
unsigned int ptt_lead_time[] = {initial_ptt_lead_time_tx1,initial_ptt_lead_time_tx2,initial_ptt_lead_time_tx3,initial_ptt_lead_time_tx4,initial_ptt_lead_time_tx5,initial_ptt_lead_time_tx6};
byte manual_ptt_invoke = 0;
byte qrss_dit_length = initial_qrss_dit_length;
byte machine_mode = 0;   // NORMAL, BEACON, COMMAND
byte char_send_mode = 0; // CW, HELL
byte key_tx = 0;         // 0 = tx_key_line control suppressed
byte dit_buffer = 0;     // used for buffering paddle hits in iambic operation
byte dah_buffer = 0;     // used for buffering paddle hits in iambic operation
byte button0_buffer = 0;
byte being_sent = 0;     // SENDING_NOTHING, SENDING_DIT, SENDING_DAH
byte key_state = 0;      // 0 = key up, 1 = key down
byte config_dirty = 0;
unsigned long ptt_time = 0; 
byte ptt_line_activated = 0;
byte speed_mode = SPEED_NORMAL;
unsigned int serial_number = 1337;
byte pause_sending_buffer = 0;
byte length_letterspace = default_length_letterspace;
byte keying_compensation = default_keying_compensation;
byte first_extension_time = default_first_extension_time;
byte ultimatic_mode = ULTIMATIC_NORMAL;
float ptt_hang_time_wordspace_units = default_ptt_hang_time_wordspace_units;
byte last_sending_type = MANUAL_SENDING;
byte zero = 0;
byte iambic_flag = 0;
unsigned long last_config_write = 0;

#ifdef FEATURE_SLEEP
unsigned long last_activity_time = 0;
#endif



#if defined(FEATURE_SERIAL)
byte incoming_serial_byte;
long serial_baud_rate;
byte cw_send_echo_inhibit = 0;
#ifdef FEATURE_COMMAND_LINE_INTERFACE
byte serial_backslash_command;
byte cli_paddle_echo = 0;
long cli_paddle_echo_buffer = 0;
unsigned long cli_paddle_echo_buffer_decode_time = 0;
byte cli_prosign_flag = 0;
byte cli_wait_for_cr_to_send_cw = 0;
#endif //FEATURE_COMMAND_LINE_INTERFACE
#endif //FEATURE_SERIAL

byte send_buffer_array[send_buffer_size];
byte send_buffer_bytes = 0;
byte send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;

#if defined(FEATURE_SERIAL)
byte serial_mode = SERIAL_NORMAL;
#endif //FEATURE_SERIAL

#define SIDETONE_HZ_LOW_LIMIT 299
#define SIDETONE_HZ_HIGH_LIMIT 2001

#if defined(DEBUG_AUX_SERIAL_PORT)
Serial_* debug_port;
#endif //DEBUG_AUX_SERIAL_PORT
Serial_* main_serial_port;

#ifdef FEATURE_PTT_INTERLOCK
byte ptt_interlock_active = 0;
#endif //FEATURE_PTT_INTERLOCK



/*  vvv  written by the original author! :O ---CL, 2015-04-06  */
//---------------------------------------------------------------------------------------------------------


// this code is a friggin work of art.  free as in beer software sucks.


//---------------------------------------------------------------------------------------------------------

void setup()
{
  initialize_pins();
  initialize_keyer_state();
  check_eeprom_for_initialization();
  check_for_beacon_mode();
  initialize_serial_port();
}

void loop()
{
  if (machine_mode == NORMAL) {
    check_paddles();
    service_dit_dah_buffers();

    #if defined(FEATURE_SERIAL)       
    check_serial();
    check_paddles();            
    service_dit_dah_buffers();
    #ifdef FEATURE_COMMAND_LINE_INTERFACE  
    service_serial_paddle_echo();
    #endif //FEATURE_COMMAND_LINE_INTERFACE
    #endif //FEATURE_SERIAL

    service_send_buffer(PRINTCHAR);
    check_ptt_tail();

    check_for_dirty_configuration();
    
    #ifdef FEATURE_SLEEP
    check_sleep();
    #endif //FEATURE_SLEEP

    #ifdef FEATURE_PTT_INTERLOCK
    service_ptt_interlock();
    #endif //FEATURE_PTT_INTERLOCK
  }
}


// Subroutines --------------------------------------------------------------------------------------------


// Are you a radio artisan ?


//-------------------------------------------------------------------------------------------------------

#ifdef HARDWARE_ARDUINO_DUE
void noTone(byte tone_pin){



}


#endif //HARDWARE_ARDUINO_DUE

//-------------------------------------------------------------------------------------------------------

#ifdef HARDWARE_ARDUINO_DUE
void tone(byte tone_pin, unsigned int tone_freq,unsigned int duration = 0){

}


#endif //HARDWARE_ARDUINO_DUE




#ifdef FEATURE_SLEEP
/*
 * ISR to wake us up from sleep.
 */
void wakeup()
{
  detachInterrupt(0);
}
#endif //FEATURE_SLEEP


#ifdef FEATURE_SLEEP
/*
 * Put the MCU in sleep mode if we've been inactive.
 */
void check_sleep()
{
  if ((millis() - last_activity_time) > (go_to_sleep_inactivity_time*60000)){
    if (config_dirty) {  // force a configuration write to EEPROM if the config is dirty
      last_config_write = 0;
      check_for_dirty_configuration();
    }
    
    attachInterrupt(0, wakeup, LOW);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();

    // ZZZZZZZZ - shhhhh! we are asleep here !!

    sleep_disable();
    last_activity_time = millis();     
  }
}
#endif //FEATURE_SLEEP


/*
 * Write the configuration to EEPROM if it is dirty.
 */
void check_for_dirty_configuration()
{
  if ((config_dirty) && ((millis()-last_config_write)>30000)) {
    write_settings_to_eeprom(0);
    last_config_write = millis();
  }
}


void check_paddles()
{
  #define NO_CLOSURE 0
  #define DIT_CLOSURE_DAH_OFF 1
  #define DAH_CLOSURE_DIT_OFF 2
  #define DIT_CLOSURE_DAH_ON 3
  #define DAH_CLOSURE_DIT_ON 4

  static byte last_closure = NO_CLOSURE;

  check_dit_paddle();
  check_dah_paddle();

  if (configuration.keyer_mode == ULTIMATIC) {
    if (ultimatic_mode == ULTIMATIC_NORMAL) {
      switch (last_closure) {
        case DIT_CLOSURE_DAH_OFF:
          if (dah_buffer) {
            if (dit_buffer) {
              last_closure = DAH_CLOSURE_DIT_ON;
              dit_buffer = 0;
            } else {
              last_closure = DAH_CLOSURE_DIT_OFF;
            }
          } else {
            if (!dit_buffer) {
              last_closure = NO_CLOSURE;
            }
          }
          break;
        case DIT_CLOSURE_DAH_ON:
          if (dit_buffer) {
            if (dah_buffer) {
              dah_buffer = 0;
            } else {
              last_closure = DIT_CLOSURE_DAH_OFF;
            }
          } else {
            if (dah_buffer) {
              last_closure = DAH_CLOSURE_DIT_OFF;
            } else {
              last_closure = NO_CLOSURE;
            }
          }
          break;

        case DAH_CLOSURE_DIT_OFF:
          if (dit_buffer) {
            if (dah_buffer) {
              last_closure = DIT_CLOSURE_DAH_ON;
              dah_buffer = 0;
            } else {
              last_closure = DIT_CLOSURE_DAH_OFF;
            }
          } else {
            if (!dah_buffer) {
              last_closure = NO_CLOSURE;
            }
          }
          break;

        case DAH_CLOSURE_DIT_ON:
          if (dah_buffer) {
            if (dit_buffer) {
              dit_buffer = 0;
            } else {
              last_closure = DAH_CLOSURE_DIT_OFF;
            }
          } else {
            if (dit_buffer) {
              last_closure = DIT_CLOSURE_DAH_OFF;
            } else {
              last_closure = NO_CLOSURE;
            }
          }
          break;

        case NO_CLOSURE:
          if ((dit_buffer) && (!dah_buffer)) {
            last_closure = DIT_CLOSURE_DAH_OFF;
          } else {
            if ((dah_buffer) && (!dit_buffer)) {
              last_closure = DAH_CLOSURE_DIT_OFF;
            } else {
              if ((dit_buffer) && (dah_buffer)) {
                // need to handle dit/dah priority here
                last_closure = DIT_CLOSURE_DAH_ON;
                dah_buffer = 0;
              }
            }
          }
          break;
      }
    } else {
     if ((dit_buffer) && (dah_buffer)) {   // dit or dah priority mode
       if (ultimatic_mode == ULTIMATIC_DIT_PRIORITY) {
         dah_buffer = 0;
       } else {
         dit_buffer = 0;
       }
     }
    }
  }
}


void ptt_key()
{
  if (ptt_line_activated == 0) {   // if PTT is currently deactivated, bring it up and insert PTT lead time delay
    if (configuration.current_ptt_line) {
      digitalWrite (configuration.current_ptt_line, HIGH);    
      delay(ptt_lead_time[configuration.current_tx-1]);
    }
    ptt_line_activated = 1;
  }
  ptt_time = millis();
}

void ptt_unkey()
{
  if (ptt_line_activated) {
    if (configuration.current_ptt_line) {
      digitalWrite (configuration.current_ptt_line, LOW);
    }
    ptt_line_activated = 0;
  }
}

void check_ptt_tail()
{
  if (key_state) {
    ptt_time = millis();
  } else {
    if ((ptt_line_activated) && (manual_ptt_invoke == 0)) {
      //if ((millis() - ptt_time) > ptt_tail_time) {
      if (last_sending_type == MANUAL_SENDING) {
        #ifndef OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING
        if ((millis() - ptt_time) >= ((configuration.length_wordspace*ptt_hang_time_wordspace_units)*float(1200/configuration.wpm)) ) {
          ptt_unkey();
        }          
        #else //ndef OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING
        #ifndef OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING
        if ((millis() - ptt_time) >= (((configuration.length_wordspace*ptt_hang_time_wordspace_units)*float(1200/configuration.wpm))+ptt_tail_time[configuration.current_tx-1])) {       
          ptt_unkey();
        }
        #else //OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING
        if ((millis() - ptt_time) >= ptt_tail_time[configuration.current_tx-1]) {       
          ptt_unkey();
        }
        #endif //OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING
        #endif //ndef OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING
      } else {
        if ((millis() - ptt_time) > ptt_tail_time[configuration.current_tx-1]) {
          #ifdef OPTION_KEEP_PTT_KEYED_WHEN_CHARS_BUFFERED
          if (!send_buffer_bytes){
            ptt_unkey();
          }
          #else
          ptt_unkey();
          #endif //OPTION_KEEP_PTT_KEYED_WHEN_CHARS_BUFFERED
        }
      }
    }
  }
}

void write_settings_to_eeprom(int initialize_eeprom)
{  
  #ifndef HARDWARE_ARDUINO_DUE  
  if (initialize_eeprom) {
    //configuration.magic_number = eeprom_magic_number;
    EEPROM.write(0,eeprom_magic_number);
    #ifdef FEATURE_MEMORIES
    initialize_eeprom_memories();
    #endif  //FEATURE_MEMORIES    
  }

  const byte* p = (const byte*)(const void*)&configuration;
  unsigned int i;
  int ee = 1;  // starting point of configuration struct
  for (i = 0; i < sizeof(configuration); i++){
    EEPROM.write(ee++, *p++);  
  }
  #endif //HARDWARE_ARDUINO_DUE
  
  config_dirty = 0;
}


int read_settings_from_eeprom()
{
  // returns 0 if eeprom had valid settings, returns 1 if eeprom needs initialized

  #ifndef HARDWARE_ARDUINO_DUE
  if (EEPROM.read(0) == eeprom_magic_number){
  
    byte* p = (byte*)(void*)&configuration;
    unsigned int i;
    int ee = 1; // starting point of configuration struct
    for (i = 0; i < sizeof(configuration); i++){
      *p++ = EEPROM.read(ee++);  
    }
  
  //if (configuration.magic_number == eeprom_magic_number) {
    switch_to_tx_silent(configuration.current_tx);
    config_dirty = 0;
    return 0;
  } else {
    return 1;
  }
  #else //HARDWARE_ARDUINO_DUE
  return 1;

  #endif //HARDWARE_ARDUINO_DUE
}


void check_dit_paddle()
{
  byte pin_value = 0;
  byte dit_paddle = 0;

  if (configuration.paddle_mode == PADDLE_NORMAL) {
    dit_paddle = paddle_left;
  } else {
    dit_paddle = paddle_right;
  }

  pin_value = paddle_pin_read(dit_paddle);

  if (pin_value == 0) {
    dit_buffer = 1;

    #ifdef FEATURE_SLEEP
    last_activity_time = millis(); 
    #endif //FEATURE_SLEEP
    manual_ptt_invoke = 0;
  }
}


void check_dah_paddle()
{
  byte pin_value = 0;
  byte dah_paddle;

  if (configuration.paddle_mode == PADDLE_NORMAL) {
    dah_paddle = paddle_right;
  } else {
    dah_paddle = paddle_left;
  }

  pin_value = paddle_pin_read(dah_paddle);
  
  if (pin_value == 0) {
    dah_buffer = 1;

    #ifdef FEATURE_SLEEP
    last_activity_time = millis(); 
    #endif //FEATURE_SLEEP    
    manual_ptt_invoke = 0;
  }
}


void send_dit(byte sending_type)
{
  unsigned int character_wpm = configuration.wpm;
  #ifdef FEATURE_FARNSWORTH
  if ((sending_type == AUTOMATIC_SENDING) && (configuration.wpm_farnsworth > configuration.wpm)) {
    character_wpm = configuration.wpm_farnsworth;
  }
  #endif //FEATURE_FARNSWORTH

  being_sent = SENDING_DIT;
  tx_and_sidetone_key(1,sending_type);
  if ((tx_key_dit) && (key_tx)) {digitalWrite(tx_key_dit,HIGH);}

  loop_element_lengths((1.0*(float(configuration.weighting)/50)),keying_compensation,character_wpm,sending_type);
  
  if ((tx_key_dit) && (key_tx)) {digitalWrite(tx_key_dit,LOW);}
  tx_and_sidetone_key(0,sending_type);

  loop_element_lengths((2.0-(float(configuration.weighting)/50)),(-1.0*keying_compensation),character_wpm,sending_type);

  #ifdef FEATURE_AUTOSPACE
  byte autospace_end_of_character_flag = 0;

  if ((sending_type == MANUAL_SENDING) && (configuration.autospace_active)) {
    check_paddles();
  }
  if ((sending_type == MANUAL_SENDING) && (configuration.autospace_active) && (dit_buffer == 0) && (dah_buffer == 0)) {
    loop_element_lengths(2,0,configuration.wpm,sending_type);
    autospace_end_of_character_flag = 1;
  }
  #endif


  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  if ((cli_paddle_echo) && (sending_type == MANUAL_SENDING)) {
    cli_paddle_echo_buffer = (cli_paddle_echo_buffer * 10) + 1;
    cli_paddle_echo_buffer_decode_time = millis() + (float((cw_echo_timing_factor*1200.0)/configuration.wpm)*length_letterspace);
  
    #ifdef FEATURE_AUTOSPACE
    if (autospace_end_of_character_flag){cli_paddle_echo_buffer_decode_time = 0;}
    #endif //FEATURE_AUTOSPACE

  }
  #endif

  #ifdef FEATURE_AUTOSPACE
  autospace_end_of_character_flag = 0;
  #endif //FEATURE_AUTOSPACE

  being_sent = SENDING_NOTHING;
  last_sending_type = sending_type;
  
  
  check_paddles();

}


void send_dah(byte sending_type)
{
  unsigned int character_wpm = configuration.wpm;
  #ifdef FEATURE_FARNSWORTH
  if ((sending_type == AUTOMATIC_SENDING) && (configuration.wpm_farnsworth > configuration.wpm)) {
    character_wpm = configuration.wpm_farnsworth;
  }
  #endif //FEATURE_FARNSWORTH

  being_sent = SENDING_DAH;
  tx_and_sidetone_key(1,sending_type);
  if ((tx_key_dah) && (key_tx)) {digitalWrite(tx_key_dah,HIGH);}

  loop_element_lengths((float(configuration.dah_to_dit_ratio/100.0)*(float(configuration.weighting)/50)),keying_compensation,character_wpm,sending_type);

  if ((tx_key_dah) && (key_tx)) {digitalWrite(tx_key_dah,LOW);}

  tx_and_sidetone_key(0,sending_type);

  loop_element_lengths((4.0-(3.0*(float(configuration.weighting)/50))),(-1.0*keying_compensation),character_wpm,sending_type);

  #ifdef FEATURE_AUTOSPACE
  byte autospace_end_of_character_flag = 0;

  if ((sending_type == MANUAL_SENDING) && (configuration.autospace_active)) {
    check_paddles();
  }
  if ((sending_type == MANUAL_SENDING) && (configuration.autospace_active) && (dit_buffer == 0) && (dah_buffer == 0)) {
    loop_element_lengths(2,0,configuration.wpm,sending_type);
    autospace_end_of_character_flag = 1;
  }
  #endif

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  if ((cli_paddle_echo) && (sending_type == MANUAL_SENDING)) {
    cli_paddle_echo_buffer = (cli_paddle_echo_buffer * 10) + 2;
    cli_paddle_echo_buffer_decode_time = millis() + (float((cw_echo_timing_factor*1200.0)/configuration.wpm)*length_letterspace);

    #ifdef FEATURE_AUTOSPACE
    if (autospace_end_of_character_flag){cli_paddle_echo_buffer_decode_time = 0;}
    #endif //FEATURE_AUTOSPACE
  }
  #endif

  #ifdef FEATURE_AUTOSPACE
  autospace_end_of_character_flag = 0;
  #endif //FEATURE_AUTOSPACE  

  check_paddles();

  being_sent = SENDING_NOTHING;
  last_sending_type = sending_type;
}


void tx_and_sidetone_key (int state, byte sending_type)
{
  #ifndef FEATURE_PTT_INTERLOCK
  if ((state) && (key_state == 0)) {
    if (key_tx) {
      byte previous_ptt_line_activated = ptt_line_activated;
      ptt_key();
      if (current_tx_key_line) {digitalWrite (current_tx_key_line, HIGH);}
      if ((first_extension_time) && (previous_ptt_line_activated == 0)) {
        delay(first_extension_time);
      }
    }
    if ((configuration.sidetone_mode == SIDETONE_ON) || (machine_mode == COMMAND) || ((configuration.sidetone_mode == SIDETONE_PADDLE_ONLY) && (sending_type == MANUAL_SENDING))) {
      tone(sidetone_line, configuration.hz_sidetone);
    }
    key_state = 1;
  } else {
    if ((state == 0) && (key_state)) {
      if (key_tx) {
        if (current_tx_key_line) {digitalWrite (current_tx_key_line, LOW);}
        ptt_key();
      }
      if ((configuration.sidetone_mode == SIDETONE_ON) || (machine_mode == COMMAND) || ((configuration.sidetone_mode == SIDETONE_PADDLE_ONLY) && (sending_type == MANUAL_SENDING))) {
        noTone(sidetone_line);
      }
      key_state = 0;
    }
  }
  #else  //FEATURE_PTT_INTERLOCK
  if ((state) && (key_state == 0)) {
    if (key_tx) {
      byte previous_ptt_line_activated = ptt_line_activated;
      if (!ptt_interlock_active) {
        ptt_key();
      }
      if (current_tx_key_line) {digitalWrite (current_tx_key_line, HIGH);}
      if ((first_extension_time) && (previous_ptt_line_activated == 0)) {
        delay(first_extension_time);
      }
    }
    if ((configuration.sidetone_mode == SIDETONE_ON) || (machine_mode == COMMAND) || ((configuration.sidetone_mode == SIDETONE_PADDLE_ONLY) && (sending_type == MANUAL_SENDING))) {
      tone(sidetone_line, configuration.hz_sidetone);
    }
    key_state = 1;
  } else {
    if ((state == 0) && (key_state)) {
      if (key_tx) {
        if (current_tx_key_line) {digitalWrite (current_tx_key_line, LOW);}
        if (!ptt_interlock_active) {
          ptt_key();
        }
      }
      if ((configuration.sidetone_mode == SIDETONE_ON) || (machine_mode == COMMAND) || ((configuration.sidetone_mode == SIDETONE_PADDLE_ONLY) && (sending_type == MANUAL_SENDING))) {
        noTone(sidetone_line);
      }
      key_state = 0;
    }
  }
  #endif //FEATURE_PTT_INTERLOCK
}


#ifndef FEATURE_HI_PRECISION_LOOP_TIMING
void loop_element_lengths(float lengths, float additional_time_ms, int speed_wpm_in, byte sending_type)
{
  if ((lengths == 0) or (lengths < 0)) {
    return;
  }

  float element_length;

  if (speed_mode == SPEED_NORMAL) {
    element_length = 1200/speed_wpm_in;
  } else {
    element_length = qrss_dit_length * 1000;
  }

  unsigned long endtime = millis() + long(element_length*lengths) + long(additional_time_ms);
  while ((millis() < endtime) && (millis() > 200)) {  // the second condition is to account for millis() rollover
    #ifdef FEATURE_PTT_INTERLOCK
    service_ptt_interlock();
    #endif //FEATURE_PTT_INTERLOCK
    
    if (configuration.keyer_mode != ULTIMATIC) {
      if ((configuration.keyer_mode == IAMBIC_A) && (paddle_pin_read(paddle_left) == LOW ) && (paddle_pin_read(paddle_right) == LOW )) {
          iambic_flag = 1;
      }    
   
      if (being_sent == SENDING_DIT) {
        check_dah_paddle();
      } else {
        if (being_sent == SENDING_DAH) {
          check_dit_paddle();
        }
      }
    }

    // blow out prematurely if we're automatic sending and a paddle gets hit
    if (sending_type == AUTOMATIC_SENDING && (paddle_pin_read(paddle_left) == LOW || paddle_pin_read(paddle_right) == LOW || dit_buffer || dah_buffer)) {
      if (machine_mode == NORMAL) {
        return;
      }
    }   
 }
 
  if ((configuration.keyer_mode == IAMBIC_A) && (iambic_flag) && (paddle_pin_read(paddle_left) == HIGH ) && (paddle_pin_read(paddle_right) == HIGH )) {
      iambic_flag = 0;
      dit_buffer = 0;
      dah_buffer = 0;
  }    
}
#else //FEATURE_HI_PRECISION_LOOP_TIMING
void loop_element_lengths(float lengths, float additional_time_ms, int speed_wpm_in, byte sending_type)
{
  if ((lengths == 0) or (lengths < 0)) {
    return;
  }

  float element_length;

  if (speed_mode == SPEED_NORMAL) {
    element_length = 1200/speed_wpm_in;
  } else {
    element_length = qrss_dit_length * 1000;
  }

  unsigned long endtime = micros() + long(element_length*lengths*1000) + long(additional_time_ms*1000);
  while ((micros() < endtime) && (micros() > 200000)) {  // the second condition is to account for millis() rollover
    #ifdef FEATURE_PTT_INTERLOCK
    service_ptt_interlock();
    #endif //FEATURE_PTT_INTERLOCK
    
    if (configuration.keyer_mode != ULTIMATIC) {
      if ((configuration.keyer_mode == IAMBIC_A) && (paddle_pin_read(paddle_left) == LOW ) && (paddle_pin_read(paddle_right) == LOW )) {
          iambic_flag = 1;
      }    
  
      if (being_sent == SENDING_DIT) {
        check_dah_paddle();
      } else {
        if (being_sent == SENDING_DAH) {
          check_dit_paddle();
        }
      }
    }

    // blow out prematurely if we're automatic sending and a paddle gets hit
    if (sending_type == AUTOMATIC_SENDING && (paddle_pin_read(paddle_left) == LOW || paddle_pin_read(paddle_right) == LOW || dit_buffer || dah_buffer)) {
    if (machine_mode == NORMAL) {
      return;
    }
  }   
 }
 
  if ((configuration.keyer_mode == IAMBIC_A) && (iambic_flag) && (paddle_pin_read(paddle_left) == HIGH ) && (paddle_pin_read(paddle_right) == HIGH )) {
      iambic_flag = 0;
      dit_buffer = 0;
      dah_buffer = 0;
  }    
}
#endif //FEATURE_HI_PRECISION_LOOP_TIMING



void speed_set(int wpm_set)
{
    configuration.wpm = wpm_set;
    config_dirty = 1;
}


long get_cw_input_from_user(unsigned int exit_time_milliseconds) {
  byte looping = 1;
  byte paddle_hit = 0;
  long cw_char = 0;
  unsigned long last_element_time = 0;
  unsigned long entry_time = millis();

  while (looping) {
    check_paddles();

    if (dit_buffer) {
      send_dit(MANUAL_SENDING);
      dit_buffer = 0;
      paddle_hit = 1;
      cw_char = (cw_char * 10) + 1;
      last_element_time = millis();
    }
    if (dah_buffer) {
      send_dah(MANUAL_SENDING);
      dah_buffer = 0;
      paddle_hit = 1;
      cw_char = (cw_char * 10) + 2;
      last_element_time = millis();
    }
    if ((paddle_hit) && (millis() > (last_element_time + (float(600/configuration.wpm) * length_letterspace)))) {
      looping = 0;
    }

    if ((!paddle_hit) && (exit_time_milliseconds) && ((millis() - entry_time) > exit_time_milliseconds)) { // if we were passed an exit time and no paddle was hit, blow out of here
      return 0;
    }

    #if defined(FEATURE_SERIAL)
    check_serial();
    #endif
  }

  return cw_char;
}


void switch_to_tx_silent(byte tx)
{
  switch (tx) {
   case 1: if ((ptt_tx_1) || (tx_key_line_1)) { configuration.current_ptt_line = ptt_tx_1; current_tx_key_line = tx_key_line_1; configuration.current_tx = 1; config_dirty = 1; } break;
   case 2: if ((ptt_tx_2) || (tx_key_line_2)) { configuration.current_ptt_line = ptt_tx_2; current_tx_key_line = tx_key_line_2; configuration.current_tx = 2; config_dirty = 1; } break;
   case 3: if ((ptt_tx_3) || (tx_key_line_3)) { configuration.current_ptt_line = ptt_tx_3; current_tx_key_line = tx_key_line_3; configuration.current_tx = 3; config_dirty = 1; } break;
   case 4: if ((ptt_tx_4) || (tx_key_line_4)) { configuration.current_ptt_line = ptt_tx_4; current_tx_key_line = tx_key_line_4; configuration.current_tx = 4; config_dirty = 1; } break;
   case 5: if ((ptt_tx_5) || (tx_key_line_5)) { configuration.current_ptt_line = ptt_tx_5; current_tx_key_line = tx_key_line_5; configuration.current_tx = 5; config_dirty = 1; } break;
   case 6: if ((ptt_tx_6) || (tx_key_line_6)) { configuration.current_ptt_line = ptt_tx_6; current_tx_key_line = tx_key_line_6; configuration.current_tx = 6; config_dirty = 1; } break;
  }
  
}


void service_dit_dah_buffers()
{
  if ((configuration.keyer_mode == IAMBIC_A) || (configuration.keyer_mode == IAMBIC_B) || (configuration.keyer_mode == ULTIMATIC)) {
    if ((configuration.keyer_mode == IAMBIC_A) && (iambic_flag) && (paddle_pin_read(paddle_left)) && (paddle_pin_read(paddle_right))) {
      iambic_flag = 0;
      dit_buffer = 0;
      dah_buffer = 0;
    } else {
      if (dit_buffer) {
        dit_buffer = 0;
        send_dit(MANUAL_SENDING);
      }
      if (dah_buffer) {
        dah_buffer = 0;
        send_dah(MANUAL_SENDING);
      }
    }
  } else {
    if (configuration.keyer_mode == BUG) {
      if (dit_buffer) {
        dit_buffer = 0;
        send_dit(MANUAL_SENDING);
      }
      if (dah_buffer) {
        dah_buffer = 0;
        tx_and_sidetone_key(1,MANUAL_SENDING);
      } else {
        tx_and_sidetone_key(0,MANUAL_SENDING);
      }
    } else {
      if (configuration.keyer_mode == STRAIGHT) {
        if (dit_buffer) {
          dit_buffer = 0;
          tx_and_sidetone_key(1,MANUAL_SENDING);
        } else {
          tx_and_sidetone_key(0,MANUAL_SENDING);
        }
      }
    }
  }
}


void beep()
{
 tone(sidetone_line, hz_high_beep, 200);
}
void boop()
{
  tone(sidetone_line, hz_low_beep);
  delay(100);
  noTone(sidetone_line);
}
void beep_boop()
{
  tone(sidetone_line, hz_high_beep);
  delay(100);
  tone(sidetone_line, hz_low_beep);
  delay(100);
  noTone(sidetone_line);
}
void boop_beep()
{
  tone(sidetone_line, hz_low_beep);
  delay(100);
  tone(sidetone_line, hz_high_beep);
  delay(100);
  noTone(sidetone_line);
}



/*
 * Send CW. Input: string, '.' for dit, '-' for dah.
 */
void send_the_dits_and_dahs(const char * cw_to_send){
  for (int x = 0;x < 12;x++){
    switch(cw_to_send[x]){
      case '.': send_dit(AUTOMATIC_SENDING); break;
      case '-': send_dah(AUTOMATIC_SENDING); break;
      default: return; break;
    }
    if ((dit_buffer) || (dah_buffer)){
      return;
    }
  }
}


void send_char(unsigned char cw_char, byte omit_letterspace)
{
  #ifdef FEATURE_SLEEP
  last_activity_time = millis(); 
  #endif //FEATURE_SLEEP

  if ((cw_char == 10) || (cw_char == 13)) { return; }  // don't attempt to send carriage return or line feed

  if (char_send_mode == CW) {
    switch (cw_char) {
      case 'A': send_the_dits_and_dahs(".-");break;
      case 'B': send_the_dits_and_dahs("-...");break;
      case 'C': send_the_dits_and_dahs("-.-.");break;
      case 'D': send_the_dits_and_dahs("-..");break;
      case 'E': send_the_dits_and_dahs(".");break;
      case 'F': send_the_dits_and_dahs("..-.");break;
      case 'G': send_the_dits_and_dahs("--.");break;
      case 'H': send_the_dits_and_dahs("....");break;
      case 'I': send_the_dits_and_dahs("..");break;
      case 'J': send_the_dits_and_dahs(".---");break;
      case 'K': send_the_dits_and_dahs("-.-");break;
      case 'L': send_the_dits_and_dahs(".-..");break;
      case 'M': send_the_dits_and_dahs("--");break;
      case 'N': send_the_dits_and_dahs("-.");break;
      case 'O': send_the_dits_and_dahs("---");break;
      case 'P': send_the_dits_and_dahs(".--.");break;
      case 'Q': send_the_dits_and_dahs("--.-");break;
      case 'R': send_the_dits_and_dahs(".-.");break;
      case 'S': send_the_dits_and_dahs("...");break;
      case 'T': send_the_dits_and_dahs("-");break;
      case 'U': send_the_dits_and_dahs("..-");break;
      case 'V': send_the_dits_and_dahs("...-");break;
      case 'W': send_the_dits_and_dahs(".--");break;
      case 'X': send_the_dits_and_dahs("-..-");break;
      case 'Y': send_the_dits_and_dahs("-.--");break;
      case 'Z': send_the_dits_and_dahs("--..");break;

      case '0': send_the_dits_and_dahs("-----");break;
      case '1': send_the_dits_and_dahs(".----");break;
      case '2': send_the_dits_and_dahs("..---");break;
      case '3': send_the_dits_and_dahs("...--");break;
      case '4': send_the_dits_and_dahs("....-");break;
      case '5': send_the_dits_and_dahs(".....");break;
      case '6': send_the_dits_and_dahs("-....");break;
      case '7': send_the_dits_and_dahs("--...");break;
      case '8': send_the_dits_and_dahs("---..");break;
      case '9': send_the_dits_and_dahs("----.");break;

      case '=': send_the_dits_and_dahs("-...-");break;
      case '/': send_the_dits_and_dahs("-..-.");break;
      case ' ': loop_element_lengths((configuration.length_wordspace-length_letterspace-2),0,configuration.wpm,AUTOMATIC_SENDING); break;
      case '*': send_the_dits_and_dahs("-...-.-");break;
      case '.': send_the_dits_and_dahs(".-.-.-");break;
      case ',': send_the_dits_and_dahs("--..--");break;
      case '\'': send_the_dits_and_dahs(".----.");break;
      case '!': send_the_dits_and_dahs("-.-.--");break;
      case '(': send_the_dits_and_dahs("-.--.");break;
      case ')': send_the_dits_and_dahs("-.--.-");break;
      case '&': send_the_dits_and_dahs(".-...");break;
      case ':': send_the_dits_and_dahs("---...");break;
      case ';': send_the_dits_and_dahs("-.-.-.");break;
      case '+': send_the_dits_and_dahs(".-.-.");break;
      case '-': send_the_dits_and_dahs("-....-");break;
      case '_': send_the_dits_and_dahs("..--.-");break;
      case '"': send_the_dits_and_dahs(".-..-.");break;
      case '$': send_the_dits_and_dahs("...-..-");break;
      case '@': send_the_dits_and_dahs(".--.-.");break;
      case '<': send_the_dits_and_dahs(".-.-.");break;
      case '>': send_the_dits_and_dahs("...-.-");break;
      case '\n':
      case '\r': break;
      
      #ifdef OPTION_NON_ENGLISH_EXTENSIONS
      case 192: send_the_dits_and_dahs(".--.-");break;
      case 194: send_the_dits_and_dahs(".-.-");break;
      case 197: send_the_dits_and_dahs(".--.-");break;
      case 196: send_the_dits_and_dahs(".-.-");break;
      case 198: send_the_dits_and_dahs(".-.-");break;
      case 199: send_the_dits_and_dahs("-.-..");break;
      case 208: send_the_dits_and_dahs("..--.");break;
      case 138: send_the_dits_and_dahs("----");break;
      case 200: send_the_dits_and_dahs(".-..-");break;
      case 201: send_the_dits_and_dahs("..-..");break;
      case 142: send_the_dits_and_dahs("--..-.");break;
      case 209: send_the_dits_and_dahs("--.--");break;
      case 214: send_the_dits_and_dahs("---.");break;
      case 216: send_the_dits_and_dahs("---.");break;
      case 211: send_the_dits_and_dahs("---.");break;
      case 220: send_the_dits_and_dahs("..--");break;
      case 223: send_the_dits_and_dahs("------");break;

      // for English/Japanese font LCD controller which has a few European characters also (HD44780UA00) (LA3ZA code)
      case 225: send_the_dits_and_dahs(".-.-");break;
      case 239: send_the_dits_and_dahs("---.");break;
      case 242: send_the_dits_and_dahs("---.");break;
      case 245: send_the_dits_and_dahs("..--");break;
      case 246: send_the_dits_and_dahs("----");break;
      case 252: send_the_dits_and_dahs(".--.-");break;
      case 238: send_the_dits_and_dahs("--.--");break;
      case 226: send_the_dits_and_dahs("------");break;
      #endif //OPTION_NON_ENGLISH_EXTENSIONS      
      
      case '|': loop_element_lengths(0.5,0,configuration.wpm,AUTOMATIC_SENDING); return; break;
      default: send_the_dits_and_dahs("..--..");break;
    }
    if (omit_letterspace != OMIT_LETTERSPACE) {
      loop_element_lengths((length_letterspace-1),0,configuration.wpm,AUTOMATIC_SENDING); //this is minus one because send_dit and send_dah have a trailing element space
    }
  } else {
      ; /* [REMOVED] (formerly Hell) */
  }
}


/*
 * ASCII lowercase -> uppercase.
 */
int uppercase (int charbytein)
{
  if (((charbytein > 96) && (charbytein < 123)) || ((charbytein > 223) && (charbytein < 255))) {
    charbytein = charbytein - 32;
  }
  if (charbytein == 158) { charbytein = 142; }  // ž -> Ž
  if (charbytein == 154) { charbytein = 138; }  // š -> Š
  
  return charbytein;
}


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_qrss_mode()
{
  int set_dit_length = serial_get_number_input(2,0,100);
  if (set_dit_length > 0) {
    qrss_dit_length = set_dit_length;
    speed_mode = SPEED_QRSS;

    main_serial_port->print(F("Setting keyer to QRSS Mode. Dit length: "));
    main_serial_port->print(set_dit_length);
    main_serial_port->println(F(" seconds"));
  }
}
#endif
#endif


/*
 * Process the CW send buffer.
 */
void service_send_buffer(byte no_print)
{
  // send one character out of the send buffer
  // values 200 and above do special things
  // 200 - SERIAL_SEND_BUFFER_WPM_CHANGE - next two bytes are new speed
  // 201 - SERIAL_SEND_BUFFER_PTT_ON
  // 202 - SERIAL_SEND_BUFFER_PTT_OFF
  // 203 - SERIAL_SEND_BUFFER_TIMED_KEY_DOWN
  // 204 - SERIAL_SEND_BUFFER_TIMED_WAIT
  // 205 - SERIAL_SEND_BUFFER_NULL
  // 206 - SERIAL_SEND_BUFFER_PROSIGN
  // 207 - SERIAL_SEND_BUFFER_HOLD_SEND
  // 208 - SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE
  // 210 - SERIAL_SEND_BUFFER_MEMORY_NUMBER - next byte is memory number to play

  static unsigned long timed_command_end_time;
  static byte timed_command_in_progress = 0;

  if (send_buffer_status == SERIAL_SEND_BUFFER_NORMAL) {
    if ((send_buffer_bytes > 0) && (pause_sending_buffer == 0)) {
      #ifdef FEATURE_SLEEP
      last_activity_time = millis(); 
      #endif //FEATURE_SLEEP
      if ((send_buffer_array[0] > SERIAL_SEND_BUFFER_SPECIAL_START) && (send_buffer_array[0] < SERIAL_SEND_BUFFER_SPECIAL_END)) {

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_HOLD_SEND) {
          send_buffer_status = SERIAL_SEND_BUFFER_HOLD;
          remove_from_send_buffer();
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE) {
          remove_from_send_buffer();
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_MEMORY_NUMBER) {
          remove_from_send_buffer();
          if (send_buffer_bytes > 0) {
            if (send_buffer_array[0] < number_of_memories) {
                ; /* [REMOVED] */
            }
            remove_from_send_buffer();
          }
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_WPM_CHANGE) {  // two bytes for wpm
          remove_from_send_buffer();
          if (send_buffer_bytes > 1) {
            configuration.wpm = send_buffer_array[0] * 256;
            remove_from_send_buffer();
            configuration.wpm = configuration.wpm + send_buffer_array[0];
            remove_from_send_buffer();
          }
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_NULL) {
          remove_from_send_buffer();
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_PROSIGN) {
          remove_from_send_buffer();
          if (send_buffer_bytes > 0) {
            send_char(send_buffer_array[0],OMIT_LETTERSPACE);
            remove_from_send_buffer();
          }
          if (send_buffer_bytes > 0) {
            send_char(send_buffer_array[0],NORMAL);
            remove_from_send_buffer();
          }
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_TIMED_KEY_DOWN) {
          remove_from_send_buffer();
          if (send_buffer_bytes > 0) {
            send_buffer_status = SERIAL_SEND_BUFFER_TIMED_COMMAND;
            tx_and_sidetone_key(1,AUTOMATIC_SENDING);
            timed_command_end_time = millis() + (send_buffer_array[0] * 1000);
            timed_command_in_progress = SERIAL_SEND_BUFFER_TIMED_KEY_DOWN;
            remove_from_send_buffer();
          }
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_TIMED_WAIT) {
          remove_from_send_buffer();
          if (send_buffer_bytes > 0) {
            send_buffer_status = SERIAL_SEND_BUFFER_TIMED_COMMAND;
            timed_command_end_time = millis() + (send_buffer_array[0] * 1000);
            timed_command_in_progress = SERIAL_SEND_BUFFER_TIMED_WAIT;
            remove_from_send_buffer();
          }
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_PTT_ON) {
          remove_from_send_buffer();
          manual_ptt_invoke = 1;
          ptt_key();
        }

        if (send_buffer_array[0] == SERIAL_SEND_BUFFER_PTT_OFF) {
          remove_from_send_buffer();
          manual_ptt_invoke = 0;

          ptt_unkey();
        }
      } else {
        #if defined(FEATURE_SERIAL)
        if ((!no_print) && (!cw_send_echo_inhibit)){
          main_serial_port->write(send_buffer_array[0]);
          if (send_buffer_array[0] == 13) {
            main_serial_port->write(10);  // if we got a carriage return, also send a line feed
          }
        }
        #endif //FEATURE_SERIAL
        send_char(send_buffer_array[0],NORMAL);
        remove_from_send_buffer();
      }
    }

  } else {

    if (send_buffer_status == SERIAL_SEND_BUFFER_TIMED_COMMAND) {    // we're in a timed command

      if ((timed_command_in_progress == SERIAL_SEND_BUFFER_TIMED_KEY_DOWN) && (millis() > timed_command_end_time)) {
        tx_and_sidetone_key(0,AUTOMATIC_SENDING);
        timed_command_in_progress = 0;
        send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;
      }

      if ((timed_command_in_progress == SERIAL_SEND_BUFFER_TIMED_WAIT) && (millis() > timed_command_end_time)) {
        timed_command_in_progress = 0;
        send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;
      }

    }

    if (send_buffer_status == SERIAL_SEND_BUFFER_HOLD) {  // we're in a send hold ; see if there's a SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE in the buffer
      if (send_buffer_bytes == 0) {
        send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;  // this should never happen, but what the hell, we'll catch it here if it ever does happen
      } else {
        for (int z = 0; z < send_buffer_bytes; z++) {
          if (send_buffer_array[z] ==  SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE) {
            send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;
            z = send_buffer_bytes;
          }
        }
      }
    }

  }

  //if the paddles are hit, dump the buffer
  check_paddles();
  if ((dit_buffer || dah_buffer) && (send_buffer_bytes  > 0)) {
    clear_send_buffer();
    send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;
    dit_buffer = 0;
    dah_buffer = 0;    
  }
}


/*
 * Clear the CW send buffer.
 */
void clear_send_buffer()
{
  send_buffer_bytes = 0;
}

/*
 * Remove the first character from the CW send buffer.
 */
void remove_from_send_buffer()
{
  if (send_buffer_bytes > 0) {
    send_buffer_bytes--;
  }
  if (send_buffer_bytes > 0) {
    for (int x = 0;x < send_buffer_bytes;x++) {
      send_buffer_array[x] = send_buffer_array[x+1];
    }
  }
}


#ifdef FEATURE_COMMAND_LINE_INTERFACE
/*
 * Add character from the serial terminal to the end of the CW send buffer.
 */
void add_to_send_buffer(byte incoming_serial_byte)
{
  if (send_buffer_bytes < send_buffer_size) {
    if (incoming_serial_byte != 127) {
      send_buffer_bytes++;
      send_buffer_array[send_buffer_bytes - 1] = incoming_serial_byte;
    } else {  // we got a backspace
      send_buffer_bytes--;
    }
  }
}
#endif //FEATURE_COMMAND_LINE_INTERFACE


#ifdef FEATURE_COMMAND_LINE_INTERFACE
/*
 * Process serial terminal input (CLI commands or CW to send)
 */
void service_command_line_interface()
{
  static byte cli_wait_for_cr_flag = 0; 
  
  if (serial_backslash_command == 0) {
    incoming_serial_byte = uppercase(incoming_serial_byte);
    if (incoming_serial_byte != 92) { // we do not have a backslash
      if (cli_prosign_flag) {
        add_to_send_buffer(SERIAL_SEND_BUFFER_PROSIGN);
        cli_prosign_flag = 0;
      }
      if (cli_wait_for_cr_to_send_cw) {
        if (cli_wait_for_cr_flag == 0) {
          if (incoming_serial_byte > 31) {
            add_to_send_buffer(SERIAL_SEND_BUFFER_HOLD_SEND);
            cli_wait_for_cr_flag = 1;
          }
        } else {
          if (incoming_serial_byte == 13) {
            add_to_send_buffer(SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE);
            cli_wait_for_cr_flag = 0;
          }
        }
      }
      add_to_send_buffer(incoming_serial_byte);
    } else {     //(incoming_serial_byte != 92)  -- we got a backslash
      serial_backslash_command = 1;
      main_serial_port->write(incoming_serial_byte);
    }
  } else { // (serial_backslash_command == 0) -- we already got a backslash
      main_serial_port->write(incoming_serial_byte);
      incoming_serial_byte = uppercase(incoming_serial_byte);
      process_serial_command();
      serial_backslash_command = 0;
      main_serial_port->println();
  }
}
#endif //FEATURE_COMMAND_LINE_INTERFACE



#if defined(FEATURE_SERIAL)
void check_serial(){
  // Reminder to Goody: multi-parameter commands must be nested in if-then-elses (see PTT command for example)

  while (main_serial_port->available() > 0) {
    incoming_serial_byte = main_serial_port->read();
    #ifdef FEATURE_SLEEP
    last_activity_time = millis(); 
    #endif //FEATURE_SLEEP    
    
    #ifndef FEATURE_COMMAND_LINE_INTERFACE
    //incoming_serial_byte = main_serial_port->read();
    main_serial_port->println(F("No serial features enabled..."));
    #endif

    // yea, this is a bit funky below

    #ifdef FEATURE_COMMAND_LINE_INTERFACE    
    service_command_line_interface();
    #endif //FEATURE_COMMAND_LINE_INTERFACE
  }
}
#endif


#if defined(FEATURE_SERIAL_HELP)
#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void print_serial_help(){
  main_serial_port->println(F("\n\rK3NG Keyer Help\n\r"));
  main_serial_port->println(F("CLI commands:"));
  main_serial_port->println(F("\\A\t\t: Iambic A"));
  main_serial_port->println(F("\\B\t\t: Iambic B"));
  main_serial_port->println(F("\\D\t\t: Ultimatic"));
  main_serial_port->println(F("\\E####\t\t: Set serial number to ####"));
  main_serial_port->println(F("\\F####\t\t: Set sidetone to #### hz"));
  main_serial_port->println(F("\\G\t\t: switch to Bug mode"));
  main_serial_port->println(F("\\I\t\t: TX line disable/enable"));
  main_serial_port->println(F("\\J###\t\t: Set Dah to Dit Ratio"));
  main_serial_port->println(F("\\L##\t\t: Set weighting (50 = normal)"));
  #ifdef FEATURE_FARNSWORTH
  main_serial_port->println(F("\\M###\t\t: Set Farnsworth Speed"));
  #endif
  main_serial_port->println(F("\\N\t\t: toggle paddle reverse"));
  main_serial_port->println(F("\\Q#[#]\t\t: Switch to QRSS mode with ## second dit length"));
  main_serial_port->println(F("\\R\t\t: Switch to regular speed (wpm) mode"));
  main_serial_port->println(F("\\S\t\t: status report"));
  main_serial_port->println(F("\\T\t\t: Tune mode"));
  main_serial_port->println(F("\\U\t\t: PTT toggle"));
  main_serial_port->println(F("\\W#[#][#]\t: Change WPM to ###"));
  main_serial_port->println(F("\\X#\t\t: Switch to transmitter #"));
  main_serial_port->println(F("\\Y#\t\t: Change wordspace to #"));
  #ifdef FEATURE_AUTOSPACE
  main_serial_port->println(F("\\Z\t\t: Autospace on/off"));
  #endif //FEATURE_AUTOSPACE
  main_serial_port->println(F("\\\\\t\t: Empty keyboard send buffer"));
}
#endif //FEATURE_COMMAND_LINE_INTERFACE
#endif //FEATURE_SERIAL
#endif //FEATURE_SERIAL_HELP



#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void process_serial_command() {
  main_serial_port->println();
  switch (incoming_serial_byte) {
    case 126: // ~ (reset)
      main_serial_port->println("Self-reset is not supported! Reset with paddles held down to clear settings."); break;
    case 42:                                                // * - paddle echo on / off
      if (cli_paddle_echo) {
        cli_paddle_echo = 0;
      } else {
        cli_paddle_echo = 1;
      }
      break;
    case 43: cli_prosign_flag = 1; break;
    #if defined(FEATURE_SERIAL_HELP)
    case 63: print_serial_help(); break;                         // ? = print help
    #endif //FEATURE_SERIAL_HELP
    case 65: configuration.keyer_mode = IAMBIC_A; config_dirty = 1; main_serial_port->println(F("Iambic A")); break;    // A - Iambic A mode
    case 66: configuration.keyer_mode = IAMBIC_B; config_dirty = 1; main_serial_port->println(F("Iambic B")); break;    // B - Iambic B mode
    case 67: char_send_mode = CW; main_serial_port->println(F("CW mode")); break;             // C - CW mode
    case 68: configuration.keyer_mode = ULTIMATIC; config_dirty = 1; main_serial_port->println(F("Ultimatic")); break;  // D - Ultimatic mode
    case 69: serial_set_serial_number(); break;                                   // E - set serial number
    case 70: serial_set_sidetone_freq(); break;                                   // F - set sidetone frequency
    case 71: configuration.keyer_mode = BUG; config_dirty = 1; main_serial_port->println(F("Bug")); break;              // G - Bug mode
    case 73:                                                                      // I - transmit line on/off
      main_serial_port->print(F("TX o"));
      if (key_tx) {
        key_tx = 0;
        main_serial_port->println(F("ff"));
      } else {
        key_tx = 1;
        main_serial_port->println(F("n"));
      }
      break;
    case 81: serial_qrss_mode(); break; // Q - activate QRSS mode
    case 82: speed_mode = SPEED_NORMAL; main_serial_port->println(F("QRSS Off")); break; // R - activate regular timing mode
    case 83: serial_status(); break;                                              // S - Status command
    case 74: serial_set_dit_to_dah_ratio(); break;                          // J - dit to dah ratio
    case 76: serial_set_weighting(); break;
    #ifdef FEATURE_FARNSWORTH
    case 77: serial_set_farnsworth(); break;                                // M - set Farnsworth speed
    #endif
    case 78:                                                                // N - paddle reverse
      main_serial_port->print(F("Paddles "));
      if (configuration.paddle_mode == PADDLE_NORMAL) {
        configuration.paddle_mode = PADDLE_REVERSE;
        main_serial_port->println(F("reversed"));
      } else {
        configuration.paddle_mode = PADDLE_NORMAL;
        main_serial_port->println(F("normal"));
      }
      config_dirty = 1;
    break;  // case 78
    case 79:                                                                // O - toggle sidetone on/off
      main_serial_port->print(F("Sidetone O"));
      if ((configuration.sidetone_mode == SIDETONE_ON) || (configuration.sidetone_mode == SIDETONE_PADDLE_ONLY)) {
        configuration.sidetone_mode = SIDETONE_OFF;
        main_serial_port->println(F("FF"));
      } else {
        configuration.sidetone_mode = SIDETONE_ON;
        main_serial_port->println(F("N"));
      }
      config_dirty = 1;
    break; // case 79
    case 84: // T - tune
      serial_tune_command(); break;
    case 85:
      main_serial_port->print(F("PTT o"));
      if (ptt_line_activated) {
        manual_ptt_invoke = 0;
        ptt_unkey();
        main_serial_port->println(F("ff"));
      } else {
        manual_ptt_invoke = 1;
        ptt_key();
        main_serial_port->println(F("n"));
      }
      break;
    case 87: serial_wpm_set();break;                                        // W - set WPM
    case 88: serial_switch_tx();break;                                      // X - switch transmitter
    case 89: serial_change_wordspace(); break;
    #ifdef FEATURE_AUTOSPACE
    case 90:
      main_serial_port->print(F("Autospace O"));
      if (configuration.autospace_active) {
        configuration.autospace_active = 0;
        config_dirty = 1;
        main_serial_port->println(F("ff"));
      } else {
        configuration.autospace_active = 1;
        config_dirty = 1;
        main_serial_port->println(F("n"));
      }
      break;
    #endif
    case 92: clear_send_buffer(); break;  // \ - clear CW send buffer
    case 94:                           // ^ - toggle send CW send immediately
       if (cli_wait_for_cr_to_send_cw) {
         cli_wait_for_cr_to_send_cw = 0;
         main_serial_port->println(F("Send CW immediately"));
       } else {
         cli_wait_for_cr_to_send_cw = 1;
         main_serial_port->println(F("Wait for CR to send CW"));
       }
      break;
    case ':':
      if (cw_send_echo_inhibit) cw_send_echo_inhibit = 0; else cw_send_echo_inhibit = 1;
      break;
    default: main_serial_port->println(F("Unknown command")); break;
  }

}
#endif //FEATURE_SERIAL
#endif //FEATURE_COMMAND_LINE_INTERFACE


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
/*
 * Write the paddle CW buffer to serial.
 */
void service_serial_paddle_echo()
{
  static byte cli_paddle_echo_space_sent = 1;

  if ((cli_paddle_echo_buffer) && (cli_paddle_echo) && (millis() > cli_paddle_echo_buffer_decode_time)) {
    main_serial_port->write(byte(convert_cw_number_to_ascii(cli_paddle_echo_buffer)));
    cli_paddle_echo_buffer = 0;
    cli_paddle_echo_buffer_decode_time = millis() + (float(600/configuration.wpm)*length_letterspace);
    cli_paddle_echo_space_sent = 0;
  }
  if ((cli_paddle_echo_buffer == 0) && (cli_paddle_echo) && (millis() > (cli_paddle_echo_buffer_decode_time + (float(1200/configuration.wpm)*(configuration.length_wordspace-length_letterspace)))) && (!cli_paddle_echo_space_sent)) {
    main_serial_port->write(" ");
    cli_paddle_echo_space_sent = 1;
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
/*
 * Take in a number as input from the serial terminal.
 */
int serial_get_number_input(byte places,int lower_limit, int upper_limit)
{
  byte incoming_serial_byte = 0;
  byte looping = 1;
  byte error = 0;
  byte numberindex = 0;
  int numbers[6];

  main_serial_port->write("> ");

  while (looping) {
    if (main_serial_port->available() == 0) {        // wait for the next keystroke
      if (machine_mode == NORMAL) {          // might as well do something while we're waiting
        check_paddles();
        service_dit_dah_buffers();
        service_send_buffer(PRINTCHAR);

        check_ptt_tail();
      }
    } else {
      incoming_serial_byte = main_serial_port->read();
      main_serial_port->write(incoming_serial_byte);
      if ((incoming_serial_byte > 47) && (incoming_serial_byte < 58)) {    // ascii 48-57 = "0" - "9")
        numbers[numberindex] = incoming_serial_byte;
        numberindex++;
        if (numberindex > places){
            looping = 0;
            error = 1;
        }
      } else {
        if (incoming_serial_byte == 13) {   // carriage return - get out
          looping = 0;
        } else {                 // bogus input - error out
          looping = 0;
          error = 1;
        }
      }
    }
  }
  main_serial_port->println("");
  if (error) {
    main_serial_port->println(F("Error..."));
    while (main_serial_port->available() > 0) { incoming_serial_byte = main_serial_port->read(); }  // clear out buffer
    return(-1);
  } else {
    int y = 1;
    int return_number = 0;
    for (int x = (numberindex - 1); x >= 0 ; x = x - 1) {
      return_number = return_number + ((numbers[x]-48) * y);
      y = y * 10;
    }
    if ((return_number > lower_limit) && (return_number < upper_limit)) {
      return(return_number);
    } else {
      main_serial_port->println(F("Error..."));
      return(-1);
    }
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_change_wordspace()
{
  int set_wordspace_to = serial_get_number_input(2,0,100);
  if (set_wordspace_to > 0) {
    config_dirty = 1;
    configuration.length_wordspace = set_wordspace_to;
    main_serial_port->write("Wordspace set to ");
    main_serial_port->println(set_wordspace_to,DEC);
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_switch_tx()
{
  int set_tx_to = serial_get_number_input(1,0,7);
  if (set_tx_to > 0) {
    switch (set_tx_to){
      case 1: switch_to_tx_silent(1); main_serial_port->print(F("Switching to TX #")); main_serial_port->println(F("1")); break;
      case 2: if ((ptt_tx_2) || (tx_key_line_2)) {switch_to_tx_silent(2); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("2")); break;
      case 3: if ((ptt_tx_3) || (tx_key_line_3)) {switch_to_tx_silent(3); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("3")); break;
      case 4: if ((ptt_tx_4) || (tx_key_line_4)) {switch_to_tx_silent(4); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("4")); break;
      case 5: if ((ptt_tx_5) || (tx_key_line_5)) {switch_to_tx_silent(5); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("5")); break;
      case 6: if ((ptt_tx_6) || (tx_key_line_6)) {switch_to_tx_silent(6); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("6")); break;
    }
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_set_dit_to_dah_ratio()
{
    int set_ratio_to = serial_get_number_input(4, 99, 1000);
    if ((set_ratio_to > 99) && (set_ratio_to < 1000)) {
      configuration.dah_to_dit_ratio = set_ratio_to;
      main_serial_port->print(F("Setting dah to dit ratio to "));
      main_serial_port->println((float(configuration.dah_to_dit_ratio)/100));
      config_dirty = 1;
    }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_set_serial_number()
{
  int set_serial_number_to = serial_get_number_input(4,0,10000);
  if (set_serial_number_to > 0) {
    serial_number = set_serial_number_to;
    main_serial_port->print(F("Setting serial number to "));
    main_serial_port->println(serial_number);
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_set_sidetone_freq()
{
  int set_sidetone_hz = serial_get_number_input(4,(SIDETONE_HZ_LOW_LIMIT-1),(SIDETONE_HZ_HIGH_LIMIT+1));
  if ((set_sidetone_hz > SIDETONE_HZ_LOW_LIMIT) && (set_sidetone_hz < SIDETONE_HZ_HIGH_LIMIT)) {
    main_serial_port->write("Setting sidetone to ");
    main_serial_port->print(set_sidetone_hz,DEC);
    main_serial_port->println(F(" hz"));
    configuration.hz_sidetone = set_sidetone_hz;
    config_dirty = 1;
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_wpm_set()
{
  int set_wpm = serial_get_number_input(3,0,1000);
  if (set_wpm > 0) {
    speed_set(set_wpm);
    main_serial_port->write("Setting WPM to ");
    main_serial_port->println(set_wpm,DEC);
    config_dirty = 1;
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
#ifdef FEATURE_FARNSWORTH
void serial_set_farnsworth()
{
  int set_farnsworth_wpm = serial_get_number_input(3,-1,1000);
  if (set_farnsworth_wpm > 0) {
    configuration.wpm_farnsworth = set_farnsworth_wpm;
    main_serial_port->write("Setting Farnworth WPM to ");
    main_serial_port->println(set_farnsworth_wpm,DEC);
    config_dirty = 1;
  }
}
#endif
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
void serial_set_weighting()
{
  int set_weighting = serial_get_number_input(2,9,91);
  if (set_weighting > 0) {
    configuration.weighting = set_weighting;
    main_serial_port->write("Setting weighting to ");
    main_serial_port->println(set_weighting,DEC);
  }
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
/*
 * Transmit continuously (for tuning purposes).
 */
void serial_tune_command ()
{
  delay(100);
  while (main_serial_port->available() > 0) {  // clear out the buffer if anything is there
    main_serial_port->read();
  }

  tx_and_sidetone_key(1,MANUAL_SENDING);
  main_serial_port->println("Keying tx - press a key to unkey");

  /* keep transmitting until a key is pressed */
  while ((main_serial_port->available() == 0))
      ;
  while (main_serial_port->available() > 0) {  // clear out the buffer if anything is there
    main_serial_port->read();
  }
  tx_and_sidetone_key(0,MANUAL_SENDING);
}
#endif
#endif


#if defined(FEATURE_SERIAL)
#ifdef FEATURE_COMMAND_LINE_INTERFACE
/*
 * Print out the keyer's configuration to serial.
 */
void serial_status()
{
  main_serial_port->println();
  switch (configuration.keyer_mode) {
    case IAMBIC_A: main_serial_port->print(F("Iambic A")); break;
    case IAMBIC_B: main_serial_port->print(F("Iambic B")); 
      break;
    case BUG: main_serial_port->print(F("Bug")); break;
    case STRAIGHT: main_serial_port->print(F("Straightkey")); break;
    case ULTIMATIC: main_serial_port->print(F("Ultimatic")); break;
  }
  main_serial_port->println();
  if (speed_mode == SPEED_NORMAL) {
    main_serial_port->print(F("WPM: "));
    main_serial_port->println(configuration.wpm,DEC);
    #ifdef FEATURE_FARNSWORTH
    main_serial_port->print(F("Farnsworth WPM: "));
    if (configuration.wpm_farnsworth < configuration.wpm) {
      main_serial_port->println(F("disabled"));
    } else {
      main_serial_port->println(configuration.wpm_farnsworth,DEC);
    }
    #endif //FEATURE_FARNSWORTH
  } else {
    main_serial_port->print(F("QRSS Mode Activated - Dit Length: "));
    main_serial_port->print(qrss_dit_length,DEC);
    main_serial_port->println(" seconds");
  }
  main_serial_port->print(F("Sidetone:"));
  switch (configuration.sidetone_mode) {
    case SIDETONE_ON: main_serial_port->print(F("ON")); break;
    case SIDETONE_OFF: main_serial_port->print(F("OFF")); break;
    case SIDETONE_PADDLE_ONLY: main_serial_port->print(F("Paddle Only")); break;
  }
  main_serial_port->print(" ");
  main_serial_port->print(configuration.hz_sidetone,DEC);
  main_serial_port->println(" hz");
  main_serial_port->print(F("Dah to dit: "));
  main_serial_port->println((float(configuration.dah_to_dit_ratio)/100));
  main_serial_port->print(F("Weighting: "));
  main_serial_port->println(configuration.weighting,DEC);
  main_serial_port->print(F("Serial Number: "));
  main_serial_port->println(serial_number,DEC);
  #ifdef FEATURE_AUTOSPACE
  main_serial_port->print(F("Autospace O"));
  if (configuration.autospace_active) {
    main_serial_port->println("n");
  } else {
    main_serial_port->println("ff");
  }
  #endif
  main_serial_port->print("Wordspace: ");
  main_serial_port->println(configuration.length_wordspace,DEC);
  main_serial_port->print("TX: ");
  main_serial_port->println(configuration.current_tx);  
}
#endif
#endif


/*
 * CW to ASCII conversion
 */
int convert_cw_number_to_ascii (long number_in)
{
 switch (number_in) {
   case 12: return 65; break;         // A
   case 2111: return 66; break;
   case 2121: return 67; break;
   case 211: return 68; break;
   case 1: return 69; break;
   case 1121: return 70; break;
   case 221: return 71; break;
   case 1111: return 72; break;
   case 11: return 73; break;
   case 1222: return 74; break;
   case 212: return 75; break;
   case 1211: return 76; break;
   case 22: return 77; break;
   case 21: return 78; break;
   case 222: return 79; break;
   case 1221: return 80; break;
   case 2212: return 81; break;
   case 121: return 82; break;
   case 111: return 83; break;
   case 2: return 84; break;
   case 112: return 85; break;
   case 1112: return 86; break;
   case 122: return 87; break;
   case 2112: return 88; break;
   case 2122: return 89; break;
   case 2211: return 90; break;    // Z

   case 22222: return 48; break;    // 0
   case 12222: return 49; break;
   case 11222: return 50; break;
   case 11122: return 51; break;
   case 11112: return 52; break;
   case 11111: return 53; break;
   case 21111: return 54; break;
   case 22111: return 55; break;
   case 22211: return 56; break;
   case 22221: return 57; break;
   case 112211: return '?'; break;  // ?
   case 21121: return 47; break;   // /
   case 2111212: return '*'; break; // BK   
   case 221122: return 44; break;  // ,
   case 121212: return '.'; break;
   case 122121: return '@'; break;
   case 222222: return 92; break;  // special hack; six dahs = \ (backslash)
   //case 2222222: return '+'; break;
   case 9: return 32; break;       // special 9 = space
   case 21112: return '='; break;
   case 12121: return '+'; break;
   #ifdef OPTION_NON_ENGLISH_EXTENSIONS
   // for English/Cyrillic/Western European font LCD controller (HD44780UA02):
   case 12212: return 197; break;     // 'Å' - AA_capital (OZ, LA, SM)
   //case 12212: return 192; break;   // 'À' - A accent   
   case 1212: return 198; break;      // 'Æ' - AE_capital   (OZ, LA)
   //case 1212: return 196; break;    // 'Ä' - A_umlaut (D, SM, OH, ...)
   case 2222: return 138; break;      // CH  - (Russian letter symbol)
   case 22122: return 209; break;     // 'Ñ' - (EA)               
   //case 2221: return 214; break;    // 'Ö' – O_umlaut  (D, SM, OH, ...)
   //case 2221: return 211; break;    // 'Ò' - O accent
   case 2221: return 216; break;      // 'Ø' - OE_capital    (OZ, LA)
   case 1122: return 220; break;      // 'Ü' - U_umlaut     (D, ...)
   case 111111: return 223; break;    // beta - double S    (D?, ...)   


   
   case 21211: return 199; break;   // Ç
   case 11221: return 208; break;   // Ð
   case 12112: return 200; break;   // È
   case 11211: return 201; break;   // É
   case 221121: return 142; break;  // Ž
   
   #endif //OPTION_NON_ENGLISH_EXTENSIONS


   //default: return 254; break;
   default: 
     #ifdef OPTION_UNKNOWN_CHARACTER_ERROR_TONE
     boop();
     #endif  //OPTION_UNKNOWN_CHARACTER_ERROR_TONE
     return unknown_cw_character; 
     break;
 }
}


/*
 * Initialize ~~~ALL THE PINS~~~
 */
void initialize_pins()
{
  /* set paddle pins to inputs with pull-up resistors */
  pinMode (paddle_left, INPUT);
  digitalWrite (paddle_left, HIGH);
  pinMode (paddle_right, INPUT);
  digitalWrite (paddle_right, HIGH);
  
  if (tx_key_line_1) {
    pinMode (tx_key_line_1, OUTPUT);
    digitalWrite (tx_key_line_1, LOW);
  }
  if (tx_key_line_2) {
    pinMode (tx_key_line_2, OUTPUT);
    digitalWrite (tx_key_line_2, LOW);
  }
  if (tx_key_line_3) {
    pinMode (tx_key_line_3, OUTPUT);
    digitalWrite (tx_key_line_3, LOW);
  }
  if (tx_key_line_4) {
    pinMode (tx_key_line_4, OUTPUT);
    digitalWrite (tx_key_line_4, LOW);
  }
  if (tx_key_line_5) {
    pinMode (tx_key_line_5, OUTPUT);
    digitalWrite (tx_key_line_5, LOW);
  }
  if (tx_key_line_6) {
    pinMode (tx_key_line_6, OUTPUT);
    digitalWrite (tx_key_line_6, LOW);
  }
    
  
  if (ptt_tx_1) {
    pinMode (ptt_tx_1, OUTPUT);
    digitalWrite (ptt_tx_1, LOW);
  }
  if (ptt_tx_2) {
    pinMode (ptt_tx_2, OUTPUT);
    digitalWrite (ptt_tx_2, LOW);
  }
  if (ptt_tx_3) {
    pinMode (ptt_tx_3, OUTPUT);
    digitalWrite (ptt_tx_3, LOW);
  }
  if (ptt_tx_4) {
    pinMode (ptt_tx_4, OUTPUT);
    digitalWrite (ptt_tx_4, LOW);
  }
  if (ptt_tx_5) {
    pinMode (ptt_tx_5, OUTPUT);
    digitalWrite (ptt_tx_5, LOW);
  }
  if (ptt_tx_6) {
    pinMode (ptt_tx_6, OUTPUT);
    digitalWrite (ptt_tx_6, LOW);
  }
  pinMode (sidetone_line, OUTPUT);
  digitalWrite (sidetone_line, LOW);

  if (tx_key_dit) {
    pinMode (tx_key_dit, OUTPUT);
    digitalWrite (tx_key_dit, LOW);
  }
  if (tx_key_dah) {
    pinMode (tx_key_dah, OUTPUT);
    digitalWrite (tx_key_dah, LOW);
  }


  #ifdef FEATURE_PTT_INTERLOCK
  pinMode(ptt_interlock,INPUT);
  if (ptt_interlock_active_state == HIGH){
    digitalWrite(ptt_interlock,LOW);
  } else {
    digitalWrite(ptt_interlock,HIGH);
  }
  #endif //FEATURE_PTT_INTERLOCK
}



/*
 * Initialize the configuration to defaults.
 */
void initialize_keyer_state()
{
  key_state = 0;
  key_tx = 1;
  configuration.wpm = initial_speed_wpm;

  configuration.hz_sidetone = initial_sidetone_freq;
  configuration.memory_repeat_time = default_memory_repeat_time;
  
  configuration.dah_to_dit_ratio = initial_dah_to_dit_ratio;
  configuration.length_wordspace = default_length_wordspace;
  configuration.weighting = default_weighting;
  
  #ifdef FEATURE_FARNSWORTH
  configuration.wpm_farnsworth = initial_speed_wpm;
  #endif //FEATURE_FARNSWORTH
  
  switch_to_tx_silent(1);

  machine_mode = NORMAL;
  configuration.paddle_mode = PADDLE_NORMAL;
  configuration.keyer_mode = IAMBIC_B;
  configuration.sidetone_mode = SIDETONE_ON;
  char_send_mode = CW;
  
  delay(250);  // wait a little bit for the caps to charge up on the paddle lines
}  


/* 
 * Either load config from EEPROM or overwrite it with defaults.
 */
void check_eeprom_for_initialization()
{
  // do an eeprom reset to defaults if paddles are squeezed
  if (paddle_pin_read(paddle_left) == LOW && paddle_pin_read(paddle_right) == LOW) {
    while (paddle_pin_read(paddle_left) == LOW && paddle_pin_read(paddle_right) == LOW)
      ; /* wait for paddle release */
    write_settings_to_eeprom(1);
    beep_boop();
    beep_boop();
    beep_boop();
  }

  // read settings from eeprom and initialize eeprom if it has never been written to
  if (read_settings_from_eeprom()) {
    write_settings_to_eeprom(1);
    beep_boop();
    beep_boop();
    beep_boop();
  }
}


/* 
 * Check for beacon mode (paddle_left == low)   [DISABLED --CL]  or straight key mode (paddle_right == low)
 */
void check_for_beacon_mode(){
  if (paddle_pin_read(paddle_left) == LOW) {
      ; /* do nothing (beacon mode has been removed) --CL */
  } else if (paddle_pin_read(paddle_right) == LOW) {
      configuration.keyer_mode = STRAIGHT;
  }
}


/*
 * Open the serial port and write boot message.
 */
void initialize_serial_port()
{
  // initialize serial port
  #if defined(FEATURE_SERIAL)

  #if defined(FEATURE_COMMAND_LINE_INTERFACE)
  serial_mode = SERIAL_NORMAL;
  serial_baud_rate = default_serial_baud_rate;
  #endif  //defined(FEATURE_COMMAND_LINE_INTERFACE)

  main_serial_port = MAIN_SERIAL_PORT;
  main_serial_port->begin(serial_baud_rate);

  #if !defined(OPTION_SUPPRESS_SERIAL_BOOT_MSG) && defined(FEATURE_COMMAND_LINE_INTERFACE)
  if (serial_mode == SERIAL_NORMAL) {
    main_serial_port->print(F("\n\rK3NG Keyer Version "));
    main_serial_port->write(CODE_VERSION);
    main_serial_port->println();
    #if defined(FEATURE_SERIAL_HELP)
    main_serial_port->println(F("\n\rEnter \\? for help\n"));
    #endif
  }
  #endif //!defined(OPTION_SUPPRESS_SERIAL_BOOT_MSG) && defined(FEATURE_COMMAND_LINE_INTERFACE)

  #endif //FEATURE_SERIAL
}


/*
 * Read a paddle pin.
 * 
 * (originally also supported capacitive touch paddles)
 */
int paddle_pin_read(int pin_to_read)
{
  return digitalRead(pin_to_read);
}


#ifdef FEATURE_PTT_INTERLOCK
/*
 * Check the PTT interlock pin and enable/disable interlock as appropriate.
 */
void service_ptt_interlock()
{
  static unsigned long last_ptt_interlock_check = 0;

  if ((millis() - last_ptt_interlock_check) > ptt_interlock_check_every_ms){
    if (digitalRead(ptt_interlock) == ptt_interlock_active_state){
      if (!ptt_interlock_active){
        ptt_interlock_active = 1;
      }
    } else {
      if (ptt_interlock_active){
        ptt_interlock_active = 0;
      }
    }
    last_ptt_interlock_check = millis();
  }
}
#endif //FEATURE_PTT_INTERLOCK
