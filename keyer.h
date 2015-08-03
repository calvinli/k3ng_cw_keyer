/*
 * keyer.h
 * ------------
 *
 * Forward declarations for k3ng_keyer.ino
 *
 */

void initialize_pins(void);
void initialize_keyer_state(void);
void check_eeprom_for_initialization(void);
void check_for_beacon_mode(void);
void initialize_serial_port(void);
void check_paddles(void);
void service_dit_dah_buffers(void);
void check_serial(void);
void service_serial_paddle_echo(void);
void service_send_buffer(void);
void check_ptt_tail(void);
void check_for_dirty_configuration(void);
void check_sleep(void);
void service_ptt_interlock(void);

void ptt_key(void);
void ptt_unkey(void);
void write_settings_to_eeprom(byte initialize_eeprom);
int read_settings_from_eeprom(void);
void check_dit_paddle(void);
void check_dah_paddle(void);
void speed_set(int wpm_set);
long get_cw_input_from_user(unsigned int exit_time_seconds);
void beep(void);
void boop(void);
void beep_boop(void);
void boop_beep(void);
void send_dits(int dits);
void send_dahs(int dahs);
int uppercase(int charbytein);
void serial_qrss_mode(void);
void clear_send_buffer(void);
void remove_from_send_buffer(void);
void service_command_line_interface(void);
void process_serial_command(void);
void serial_change_wordspace(void);
void serial_switch_tx(void);
void serial_set_dit_to_dah_ratio(void);
void serial_set_serial_number(void);
void serial_set_sidetone_freq(void);
void serial_wpm_set(void);
void serial_set_weighting(void);
void serial_tune_command(void);
void serial_status(void);
int convert_cw_number_to_ascii(long number_in);
void initialize_eeprom_memories(void);
void add_to_send_buffer(byte);
void loop_element_lengths(float lengths, float additional_time_ms, int speed_wpm_in);
int serial_get_number_input(byte places,int lower_limit, int upper_limit);
void serial_set_farnsworth(void);
void wakeup(void);

int paddle_pin_read(int pin_to_read);
void send_char(char cw_char, byte omit_letterspace);
void tx_and_sidetone_key (int state);
void switch_to_tx_silent(byte tx);
