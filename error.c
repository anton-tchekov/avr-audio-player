/* Error Messages */
static const char error_sd[] PROGMEM = "SD-Card Error";
static const char error_fat[] PROGMEM = "FAT32 Error";
static const char error_io[] PROGMEM = "I/O Error";

static void mode_error(const char *msg)
{
	static const char notice[] PROGMEM = "Reset required";
	player_audio_stop();
	lcd_clear();

	/* First Line */
	lcd_string_P(msg);

	/* Second Line */
	lcd_cursor(0, 1);
	lcd_string_P(notice);
	for(;;) ;
}
