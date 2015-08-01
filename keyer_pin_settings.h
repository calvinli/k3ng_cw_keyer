/* Pins - you must review these and configure ! */
#ifndef keyer_pin_settings_h
#define paddle_left 2
#define paddle_right 3
#define tx_key_line_1 4       // (high = key down/tx on)
#define tx_key_line_2 13       // LED!!!
#define tx_key_line_3 0
#define tx_key_line_4 0
#define tx_key_line_5 0
#define tx_key_line_6 0
#define sidetone_line 5         // connect a speaker for sidetone
#define potentiometer A0        // Speed potentiometer (0 to 5 V) Use pot from 1k to 10k
#define ptt_tx_1 6              // PTT ("push to talk") lines
#define ptt_tx_2 6              //   Can be used for keying fox transmitter, T/R switch, or keying slow boatanchors
#define ptt_tx_3 0              //   These are optional - set to 0 if unused
#define ptt_tx_4 0
#define ptt_tx_5 0
#define ptt_tx_6 0
#define cw_decoder_pin A5 //A3
#define tx_key_dit 0            // if defined, goes high for dit (any transmitter)
#define tx_key_dah 0            // if defined, goes high for dah (any transmitter)
#endif //keyer_pin_settings_h

#ifdef FEATURE_PTT_INTERLOCK
#define ptt_interlock 0  // this pin disables PTT and TX KEY
#endif //FEATURE_PTT_INTERLOCK

