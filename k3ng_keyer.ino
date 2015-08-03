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
#define eeprom_magic_number 29

#include <stdio.h>

#include <EEPROM.h>

// necessary for arduino-mk; remove for IDE
#include "keyer.h"

#include "keyer_features_and_options.h"
#include "keyer_pin_settings.h"
#include "keyer_settings.h"

#if defined(FEATURE_COMMAND_LINE_INTERFACE)
 #define FEATURE_SERIAL
#endif

// Variables and stuff
struct config_t {  // 23 bytes
  unsigned int wpm;
  byte paddle_mode; /* Normal or Reversed */
  byte keyer_mode;  /* Iambic A or Iambic B */
  unsigned int hz_sidetone;
  unsigned int dah_to_dit_ratio;
  byte length_wordspace;
  byte current_ptt_line;
  byte current_tx;
  byte weighting;
  byte dit_buffer_off;
  byte dah_buffer_off;
} configuration;


byte command_mode_disable_tx = 0;
byte current_tx_key_line = tx_key_line_1;
byte manual_ptt_invoke = 0;
byte machine_mode = 0;   // NORMAL, COMMAND
byte key_tx = 0;         // 0 = tx_key_line control suppressed
byte dit_buffer = 0;     // used for buffering paddle hits in iambic operation
byte dah_buffer = 0;     // used for buffering paddle hits in iambic operation
byte button0_buffer = 0;
byte being_sent = 0;     // SENDING_NOTHING, SENDING_DIT, SENDING_DAH
byte key_state = 0;      // 0 = key up, 1 = key down
byte config_dirty = 0;
unsigned long ptt_time = 0;
byte ptt_line_activated = 0;
byte pause_sending_buffer = 0;
byte length_letterspace = default_length_letterspace;
byte keying_compensation = default_keying_compensation;
byte first_extension_time = default_first_extension_time;
byte iambic_flag = 0;
unsigned long last_config_write = 0;


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


#define SIDETONE_HZ_LOW_LIMIT 299
#define SIDETONE_HZ_HIGH_LIMIT 2001

Serial_* main_serial_port;

/*  vvv  written by the original author! :O ---CL, 2015-04-06  */
//------------------------------------------------------------------------------


// this code is a friggin work of art.  free as in beer software sucks.


//------------------------------------------------------------------------------

void setup()
{
  initialize_pins();
  initialize_keyer_state();
  check_eeprom_for_initialization();
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

    service_send_buffer();
    check_ptt_tail();

    check_for_dirty_configuration();
  }
}


// Subroutines --------------------------------------------------------------------------------------------


// Are you a radio artisan ?


//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------

void check_for_dirty_configuration()
{
  if ((config_dirty) && ((millis()-last_config_write)>30000)) {
    write_settings_to_eeprom(0);
    last_config_write = millis();
  }
}

void check_paddles()
{
  check_dit_paddle();
  check_dah_paddle();
}

void ptt_key()
{
  if (ptt_line_activated == 0) {   // if PTT is currently deactivated, bring it up and insert PTT lead time delay
    if (configuration.current_ptt_line) {
      digitalWrite (configuration.current_ptt_line, HIGH);
      delay(0); // formerly ptt lead time
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
  } else if (ptt_line_activated &&
             !manual_ptt_invoke &&
             (millis() - ptt_time) >= 10) {
    ptt_unkey();
  }
}

void write_settings_to_eeprom(byte initialize_eeprom)
{
  if (initialize_eeprom) {
    EEPROM.write(0,eeprom_magic_number);
  }

  const byte* p = (const byte*)(const void*)&configuration;
  unsigned int i;
  int ee = 1;  // starting point of configuration struct
  for (i = 0; i < sizeof(configuration); i++){
    EEPROM.write(ee++, *p++);
  }
}

int read_settings_from_eeprom()
{
  if (EEPROM.read(0) == eeprom_magic_number){
    byte* p = (byte*)(void*)&configuration;
    unsigned int i;
    int ee = 1; // starting point of configuration struct
    for (i = 0; i < sizeof(configuration); i++){
      *p++ = EEPROM.read(ee++);
    }

    switch_to_tx_silent(configuration.current_tx);
    config_dirty = 0;
    return 0;
  } else {
    return 1;
  }
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

    manual_ptt_invoke = 0;
  }
}

#warning WHY THE FLOATS
void send_dit()
{
  unsigned int character_wpm = configuration.wpm;

  being_sent = SENDING_DIT;
  tx_and_sidetone_key(1);

  loop_element_lengths((1.0*(float(configuration.weighting)/50)),keying_compensation,character_wpm);

  tx_and_sidetone_key(0);

  loop_element_lengths((2.0-(float(configuration.weighting)/50)),(-1.0*keying_compensation),character_wpm);

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  if (cli_paddle_echo) {
    cli_paddle_echo_buffer = (cli_paddle_echo_buffer * 10) + 1;
    cli_paddle_echo_buffer_decode_time = millis() + (float((cw_echo_timing_factor*1200.0)/configuration.wpm)*length_letterspace);
  }
  #endif

  being_sent = SENDING_NOTHING;

  check_paddles();

}

void send_dah()
{
  unsigned int character_wpm = configuration.wpm;

  being_sent = SENDING_DAH;
  tx_and_sidetone_key(1);

  loop_element_lengths((float(configuration.dah_to_dit_ratio/100.0)*(float(configuration.weighting)/50)),keying_compensation,character_wpm);

  tx_and_sidetone_key(0);

  loop_element_lengths((4.0-(3.0*(float(configuration.weighting)/50))),(-1.0*keying_compensation),character_wpm);

  #ifdef FEATURE_COMMAND_LINE_INTERFACE
  if (cli_paddle_echo) {
    cli_paddle_echo_buffer = (cli_paddle_echo_buffer * 10) + 2;
    cli_paddle_echo_buffer_decode_time = millis() + (float((cw_echo_timing_factor*1200.0)/configuration.wpm)*length_letterspace);
  }
  #endif

  check_paddles();

  being_sent = SENDING_NOTHING;
}

#warning WHY IS THIS SO COMPLICATED
void tx_and_sidetone_key (int state)
{
  if ((state) && (key_state == 0)) {
    if (key_tx) {
      byte previous_ptt_line_activated = ptt_line_activated;
      ptt_key();
      if (current_tx_key_line) {digitalWrite (current_tx_key_line, HIGH);}
      if ((first_extension_time) && (previous_ptt_line_activated == 0)) {
        delay(first_extension_time);
      }
    }
    tone(sidetone_line, configuration.hz_sidetone);
    key_state = 1;
  } else {
    if ((state == 0) && (key_state)) {
      if (key_tx) {
        if (current_tx_key_line) {digitalWrite (current_tx_key_line, LOW);}
        ptt_key();
      }
      noTone(sidetone_line);
      key_state = 0;
    }
  }
}

#warning WHY ARE THERE SO MANY FLOATS HERE?!?!?!?!?
void loop_element_lengths(float lengths, float additional_time_ms, int speed_wpm_in)
{
  if ((lengths == 0) or (lengths < 0)) {
    return;
  }

  float element_length;

  element_length = 1200/speed_wpm_in;

  unsigned long endtime = millis() + long(element_length*lengths) + long(additional_time_ms);
  while ((millis() < endtime) && (millis() > 200)) {  // the second condition is to account for millis() rollover

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

  if ((configuration.keyer_mode == IAMBIC_A) && (iambic_flag) && (paddle_pin_read(paddle_left) == HIGH ) && (paddle_pin_read(paddle_right) == HIGH )) {
      iambic_flag = 0;
      dit_buffer = 0;
      dah_buffer = 0;
  }
}

void speed_set(int wpm_set)
{
    configuration.wpm = wpm_set;
    config_dirty = 1;
}

long get_cw_input_from_user(unsigned int exit_time_milliseconds)
{
  byte looping = 1;
  byte paddle_hit = 0;
  long cw_char = 0;
  unsigned long last_element_time = 0;
  unsigned long entry_time = millis();

  while (looping) {
    check_paddles();

    if (dit_buffer) {
      send_dit();
      dit_buffer = 0;
      paddle_hit = 1;
      cw_char = (cw_char * 10) + 1;
      last_element_time = millis();
    }
    if (dah_buffer) {
      send_dah();
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
  }

}

void service_dit_dah_buffers()
{
  if ((configuration.keyer_mode == IAMBIC_A) && (iambic_flag) && (paddle_pin_read(paddle_left)) && (paddle_pin_read(paddle_right))) {
    iambic_flag = 0;
    dit_buffer = 0;
    dah_buffer = 0;
  } else {
    if (dit_buffer) {
      dit_buffer = 0;
      send_dit();
    }
    if (dah_buffer) {
      dah_buffer = 0;
      send_dah();
    }
  }
}

void beep()
{
 tone(sidetone_line, hz_high_beep, 200);
}
void boop()
{
  tone(sidetone_line, hz_low_beep, 100);
}

/*
 * Send CW. Input: string, '.' for dit, '-' for dah.
 */
void send_the_dits_and_dahs(const char * cw_to_send){
  for (int x = 0;x < 12;x++){
    switch(cw_to_send[x]){
      case '.': send_dit(); break;
      case '-': send_dah(); break;
      case ' ': loop_element_lengths((configuration.length_wordspace-length_letterspace-2),0,configuration.wpm); break;
      default: return; break;
    }
    if ((dit_buffer) || (dah_buffer)){
      return;
    }
  }
}

#warning CHECK THIS MORSE CODE TABLE VERY CAREFULLY
/* Offset: 32 (first entry is space) */
const char* MORSE_CODE[] = {
" ",       // [ ]
"-.-.--",  // !
".-..-.",  // "
"..--..",  // # (?)
"...-..-", // $ (SX)
"..--..",  // % (?)
".-...",   // & (AS)
".----.",  // '
"-.--.",   // (
"-.--.-",  // )
"-...-.-", // * (BK)
".-.-.",   // + (AR)
"--..--",  // ,
"-....-",  // -
".-.-.-",  // .
"-..-.",   // /
"-----",   // 0
".----",   // 1
"..---",   // 2
"...--",   // 3
"....-",   // 4
".....",   // 5
"-....",   // 6
"--...",   // 7
"---..",   // 8
"----.",   // 9
"---...",  // :
"-.-.-.",  // ;
".-.-.",   // < (AR)
"-...-",   // =
"...-.-",  // > (SK)
"..--..",   // ? (actually)
".--.-.",   // @ (AC)
".-",      // A
"-...",
"-.-.",
"-..",
".",
"..-.",
"--.",
"....",
"..",
".---",
"-.-",
".-..",
"--",
"-.",
"---",
".--.",
"--.-",
".-.",
"...",
"-",
"..-",
"..._",
".__",
"-..-",
"-.--",
"--..",    // Z
};

void send_char(unsigned char cw_char, byte omit_letterspace)
{
  if ((cw_char == '\n') || (cw_char == '\r')) { return; }

  #warning CHECK THIS MORSE CODE TABLE LOOKUP VERY CAREFULLY
  if (cw_char >= 32 && cw_char <= 90) { /* between space and Z */
    send_the_dits_and_dahs(MORSE_CODE[cw_char-32]);
  } else {
    send_the_dits_and_dahs(MORSE_CODE['?'-32]);
  }

  if (omit_letterspace != OMIT_LETTERSPACE) {
    loop_element_lengths((length_letterspace-1),0,configuration.wpm);
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
  return charbytein;
}

#warning WHY IS THIS SO COMPLICATED
/*
 * Process the CW send buffer.
 */
void service_send_buffer()
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
            tx_and_sidetone_key(1);
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
        if (!cw_send_echo_inhibit || send_buffer_array[0] == 13 || send_buffer_array[0] == 32){
          main_serial_port->write(send_buffer_array[0]);
        }
        if (send_buffer_array[0] == 13) {
          main_serial_port->write(10);  // if we got a carriage return, also send a line feed
        }
        #endif //FEATURE_SERIAL
        send_char(send_buffer_array[0],NORMAL);
        remove_from_send_buffer();
      }
    }

  } else {

    if (send_buffer_status == SERIAL_SEND_BUFFER_TIMED_COMMAND) {    // we're in a timed command

      if ((timed_command_in_progress == SERIAL_SEND_BUFFER_TIMED_KEY_DOWN) && (millis() > timed_command_end_time)) {
        tx_and_sidetone_key(0);
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
  main_serial_port->println(F("\\A\t\t: Iambic A"));
  main_serial_port->println(F("\\B\t\t: Iambic B"));
  main_serial_port->println(F("\\F####\t\t: Set sidetone to #### hz"));
  main_serial_port->println(F("\\N\t\t: toggle paddle reverse"));
  main_serial_port->println(F("\\S\t\t: status report"));
  main_serial_port->println(F("\\W#[#][#]\t: Change WPM to ###"));
  main_serial_port->println(F("\\X#\t\t: Switch to transmitter #"));
  main_serial_port->println(F("\\*\t\t: Toggle paddle echo"));
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
    case 42: // * - paddle echo on / off
       cli_paddle_echo = 1 - cli_paddle_echo;
       cw_send_echo_inhibit = 1 - cw_send_echo_inhibit;
      break;
    case 43: cli_prosign_flag = 1; break;
    #if defined(FEATURE_SERIAL_HELP)
    case 63: print_serial_help(); break;                         // ? = print help
    #endif //FEATURE_SERIAL_HELP
    case 65: configuration.keyer_mode = IAMBIC_A; config_dirty = 1; main_serial_port->println(F("Iambic A")); break;    // A - Iambic A mode
    case 66: configuration.keyer_mode = IAMBIC_B; config_dirty = 1; main_serial_port->println(F("Iambic B")); break;    // B - Iambic B mode
    case 70: serial_set_sidetone_freq(); break;                                   // F - set sidetone frequency
    case 83: serial_status(); break;                                              // S - Status command
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
    case 87: serial_wpm_set();break;                                        // W - set WPM
    case 88: serial_switch_tx();break;                                      // X - switch transmitter
    case 92: clear_send_buffer(); break;  // \ - clear CW send buffer
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
        service_send_buffer();

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
void serial_switch_tx()
{
  int set_tx_to = serial_get_number_input(1,0,7);
  if (set_tx_to > 0) {
    switch (set_tx_to){
      case 1: switch_to_tx_silent(1); main_serial_port->print(F("Switching to TX #")); main_serial_port->println(F("1")); break;
      case 2: if ((ptt_tx_2) || (tx_key_line_2)) {switch_to_tx_silent(2); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("2")); break;
      case 3: if ((ptt_tx_3) || (tx_key_line_3)) {switch_to_tx_silent(3); main_serial_port->print(F("Switching to TX #"));} main_serial_port->println(F("3")); break;
    }
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
    main_serial_port->println(F(" Hz"));
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
/*
 * Print out the keyer's configuration to serial.
 */
void serial_status()
{
  main_serial_port->print(F("Paddles: "));
  if (configuration.paddle_mode == PADDLE_NORMAL) {
    main_serial_port->println(F("normal"));
  } else {
    main_serial_port->println(F("reversed"));
  }

  main_serial_port->print(F("Mode: "));
  switch (configuration.keyer_mode) {
    case IAMBIC_A: main_serial_port->println(F("Iambic A")); break;
    case IAMBIC_B: main_serial_port->println(F("Iambic B")); break;
  }

  main_serial_port->print(F("WPM: "));
  main_serial_port->println(configuration.wpm,DEC);

  main_serial_port->print(F("Sidetone: "));
  main_serial_port->print(configuration.hz_sidetone,DEC);
  main_serial_port->println(" Hz");

  main_serial_port->print("TX: ");
  main_serial_port->println(configuration.current_tx);
}
#endif
#endif

#warning is this really necessary
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

   default:
     boop();
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

  pinMode (sidetone_line, OUTPUT);
  digitalWrite (sidetone_line, LOW);

  // something is causing the LED to go high at start. undo that.
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
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

  configuration.dah_to_dit_ratio = initial_dah_to_dit_ratio;
  configuration.length_wordspace = default_length_wordspace;
  configuration.weighting = default_weighting;

  switch_to_tx_silent(1);

  machine_mode = NORMAL;
  configuration.paddle_mode = PADDLE_NORMAL;
  configuration.keyer_mode = IAMBIC_A;

  delay(100);
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
    beep(); boop();
    beep(); boop();
  }

  // read settings from eeprom and initialize eeprom if it has never been written to
  if (read_settings_from_eeprom()) {
    write_settings_to_eeprom(1);
    beep(); boop();
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
  serial_baud_rate = default_serial_baud_rate;
  #endif  //defined(FEATURE_COMMAND_LINE_INTERFACE)

  main_serial_port = MAIN_SERIAL_PORT;
  main_serial_port->begin(serial_baud_rate);

  #if !defined(OPTION_SUPPRESS_SERIAL_BOOT_MSG) && defined(FEATURE_COMMAND_LINE_INTERFACE)
  main_serial_port->print(F("\n\rK3NG Keyer Version "));
  main_serial_port->write(CODE_VERSION);
  main_serial_port->println();
  #if defined(FEATURE_SERIAL_HELP)
  main_serial_port->println(F("\n\rEnter \\? for help\n"));
  #endif
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
