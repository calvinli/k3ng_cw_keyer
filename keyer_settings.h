// Initial and hardcoded settings
#define initial_speed_wpm 18             // "factory default" keyer speed setting
#define initial_sidetone_freq 600        // "factory default" sidetone frequency setting
#define hz_high_beep 1500                // frequency in hertz of high beep
#define hz_low_beep 400                  // frequency in hertz of low beep
#define initial_dah_to_dit_ratio 300     // 300 = 3 / normal 3:1 ratio
#define wpm_limit_low 5
#define wpm_limit_high 60
#define default_serial_baud_rate 115200
#define send_buffer_size 150
#define default_length_letterspace 3
#define default_length_wordspace 7
#define default_keying_compensation 0    // number of milliseconds to extend all dits and dahs - for QSK on boatanchors
#define default_first_extension_time 0   // number of milliseconds to extend first sent dit or dah
#define default_weighting 50             // 50 = weighting factor of 1 (normal)
#define cw_echo_timing_factor 0.25
#define unknown_cw_character '*'
#define number_of_memories byte(12)

// Variable macros
#define STRAIGHT 1
#define IAMBIC_B 2
#define IAMBIC_A 3

#define PADDLE_NORMAL 0
#define PADDLE_REVERSE 1

#define NORMAL 0
#define BEACON 1
#define COMMAND 2

#define OMIT_LETTERSPACE 1

#define SIDETONE_OFF 0
#define SIDETONE_ON 1
#define SIDETONE_PADDLE_ONLY 2

#define SENDING_NOTHING 0
#define SENDING_DIT 1
#define SENDING_DAH 2

#define SERIAL_SEND_BUFFER_SPECIAL_START 13
#define SERIAL_SEND_BUFFER_WPM_CHANGE 14
#define SERIAL_SEND_BUFFER_PTT_ON 15
#define SERIAL_SEND_BUFFER_PTT_OFF 16
#define SERIAL_SEND_BUFFER_TIMED_KEY_DOWN 17
#define SERIAL_SEND_BUFFER_TIMED_WAIT 18
#define SERIAL_SEND_BUFFER_NULL 19
#define SERIAL_SEND_BUFFER_PROSIGN 20
#define SERIAL_SEND_BUFFER_HOLD_SEND 21
#define SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE 22
#define SERIAL_SEND_BUFFER_MEMORY_NUMBER 23
#define SERIAL_SEND_BUFFER_SPECIAL_END 24

#define SERIAL_SEND_BUFFER_NORMAL 0
#define SERIAL_SEND_BUFFER_TIMED_COMMAND 1
#define SERIAL_SEND_BUFFER_HOLD 2

#define AUTOMATIC_SENDING 0
#define MANUAL_SENDING 1


#define PRINTCHAR 0
#define NOPRINT 1

#define MAIN_SERIAL_PORT &Serial
