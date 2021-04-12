/* AVR Audio Player/Recorder
 * Channels: 1 (Mono), 2 (Stereo)
 * Bit resolutions: 8 bit
 * PWM Timer0 Frequency: 16 000 000 Hz (F_CPU) / 256 = 62 500 Hz (MAX)
 */

#include <stdint.h>

#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define ARRAY_LENGTH(a)        (sizeof(a) / sizeof(*a))

#include "lcd.c"
#include "button.c"
#include "util.c"
#include "sd.c"
#include "fat.c"
#include "chars.c"

#include "player.c"
#include "error.c"
/* #include "recorder.c" */
#include "dir.c"

int main(void)
{
	lcd_init();
	gchar_init();
	button_init();
	player_audio_init();

	if(sd_init())
	{
		mode_error(error_sd);
	}

	if(fat_mount())
	{
		mode_error(error_fat);
	}

	dir_open();

	/*player_audio_play("RAILGUN.WAV");

	{
		static const char msg[] PROGMEM = "Playback ended";
		lcd_clear();
		lcd_string_P(msg);
	}
	*/

	for(;;)
	{

	}

	return 0;
}
