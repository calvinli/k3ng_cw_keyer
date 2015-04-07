// compile time features and options - comment or uncomment to add or delete features
// FEATURES add more bytes to the compiled binary, OPTIONS change code behavior

#define FEATURE_COMMAND_LINE_INTERFACE        // (this no longer requires FEATURE_SERIAL)
#define FEATURE_SERIAL_HELP

//#define FEATURE_AUTOSPACE
//#define FEATURE_FARNSWORTH
//#define FEATURE_SLEEP                   // go to sleep after x minutes to conserve battery power
//#define FEATURE_HI_PRECISION_LOOP_TIMING
//#define FEATURE_PTT_INTERLOCK 

#define OPTION_SUPPRESS_SERIAL_BOOT_MSG
#define OPTION_INCLUDE_PTT_TAIL_FOR_MANUAL_SENDING
#define OPTION_EXCLUDE_PTT_HANG_TIME_FOR_MANUAL_SENDING
//#define OPTION_NON_ENGLISH_EXTENSIONS  // add support for additional CW characters (i.e. À, Å, Þ, etc.)
//#define OPTION_KEEP_PTT_KEYED_WHEN_CHARS_BUFFERED    // this option keeps PTT high if there are characters buffered from the keyboard, the serial interface, or Winkey
//#define OPTION_UNKNOWN_CHARACTER_ERROR_TONE
#define OPTION_DO_NOT_SAY_HI
