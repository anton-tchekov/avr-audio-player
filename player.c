#include <stdio.h>

/* [ DEFINES ] */

/* Headphone stereo channels */
#define PLAYER_PIN_CHANNEL_RIGHT     5
#define PLAYER_PIN_CHANNEL_LEFT      6

/* Number of skip intervals */
#define PLAYER_NUM_INTERVALS         6

#define PLAYER_BUFFER_SIZE         512

/* [ VARIABLES ] */

/* Sample Rate - Prescaler Mapping */
struct
{
	uint32_t sample_rate;
	uint8_t compare_value;
}
/* OCR values for Timer2 for different sample rates */
static player_sample_rates[] =
{
	{  8000, 250 },
	{ 11025, 181 },
	{ 16000, 125 },
	{ 22050,  91 },
	{ 24000,  83 },
	{ 32000,  63 },
	{ 44100,  45 },
	{ 48000,  42 }
};

/* WAV Info */
struct
{
	uint8_t num_channels, compare_value, lh, lm, ls, ph, pm, ps;
	uint16_t offset;
	uint32_t data_len, sample_rate;
}
static volatile player_wi;

struct
{
	uint8_t Exit : 1;
	uint8_t Paused : 1;
	uint8_t Rewind : 1;
	uint8_t SkipFwd : 1;
	uint8_t SkipBwd : 1;
}
static player_flags;

static uint8_t player_skip_idx;

/* Double Buffering */
static uint8_t player_buf[2 * PLAYER_BUFFER_SIZE];
static uint16_t player_buf_idx = 0;

/* Set when the buffers are swapped */
static volatile uint8_t player_swap_flag;
static volatile uint16_t player_read_offset, player_write_offset;

static const char player_skip_interval_str[] PROGMEM = "05s" "10s" "30s" "05m" "10m" "30m";
static uint16_t player_skip_amount[6] = { 5, 10, 30, 300, 600, 1800 };

/* [ FUNCTIONS ] */
static void player_audio_init(void);
static uint8_t player_wav_info(void);
static void player_audio_start(void);
static void player_audio_stop(void);
static void player_event(uint8_t button);

static void player_render_pause(void);
static void player_render_name(const char *name);
static void player_render_plus_minus(void);
static void player_render_interval(void);
static void player_render_timer(void);
static void player_render_timer_partial(void);
static void player_render_init(const char *name);

/* Timer 0: Noninverting Fast PWM on PD5 and PD6 (62.5 KHz) */
/* Timer 2: CTC Mode, Interrupt, Frequency is set separately */
/* Set Pins of the left and the right Channel to output */
/* Timer0: No Prescaler, Timer2: Prescaler F_CPU / 8 */
static void player_audio_init(void)
{
	DDRD |= (1 << PLAYER_PIN_CHANNEL_RIGHT) | (1 << PLAYER_PIN_CHANNEL_LEFT);
}

static uint8_t player_wav_info(void)
{
	{
		uint16_t n;
		if(fat_fread(player_buf, PLAYER_BUFFER_SIZE, &n) || n != PLAYER_BUFFER_SIZE)
		{
			return 1;
		}
	}

	if(!mem_cmp(player_buf, (uint8_t *)"RIFF", 4))
	{
		return 1;
	}

	if(!mem_cmp(player_buf + 8, (uint8_t *)"WAVEfmt ", 8))
	{
		return 1;
	}

	/* Subchunk1Size - PCM */
	if(ld_u32(player_buf + 16) != 16)
	{
		return 1;
	}

	/* AudioFormat - Linear Quantization */
	if(ld_u16(player_buf + 20) != 1)
	{
		return 1;
	}

	/* NumChannels */
	player_wi.num_channels = ld_u16(player_buf + 22);
	if(player_wi.num_channels != 1 && player_wi.num_channels != 2)
	{
		return 1;
	}

	{
		/* SampleRate */
		uint8_t i;
		uint32_t sample_rate;
		sample_rate = ld_u32(player_buf + 24);
		for(i = 0; i < ARRAY_LENGTH(player_sample_rates); ++i)
		{
			if(sample_rate == player_sample_rates[i].sample_rate)
			{
				player_wi.compare_value = player_sample_rates[i].compare_value;
				break;
			}
		}

		player_wi.sample_rate = sample_rate;
		if(i == ARRAY_LENGTH(player_sample_rates))
		{
			return 1;
		}
	}

	{
		const uint8_t *data_ptr;
		if(!(data_ptr = mem_mem(player_buf + 36, PLAYER_BUFFER_SIZE - 36, (uint8_t *)"data", 4)))
		{
			return 1;
		}

		player_wi.offset = data_ptr - player_buf + 8;
		player_wi.data_len = ld_u32(data_ptr + 4);
	}

	return 0;
}

static void player_audio_start(void)
{
	TCCR0B = (1 << CS00);
	TCCR2B = (1 << CS21);
	TCCR2A = (1 << WGM21);
	TIMSK2 = (1 << OCIE2A);
	TCCR0A = (1 << COM0A1) | (1 << COM0B1) |
		(1 << WGM01) | (1 << WGM00);
	OCR0A = 0;
	OCR0B = 0;
	sei();
}

static void player_audio_stop(void)
{
	TCCR0B = 0;
	TCCR2B = 0;
	TCCR2A = 0;
	TIMSK2 = 0;
	TCCR0A = 0;
	OCR0A = 0;
	OCR0B = 0;
	cli();
}

static void player_event(uint8_t button)
{
	switch(button)
	{
		/* Row 1 */
		case 0:
			/* Play/Pause */
			if(player_flags.Paused)
			{
				player_flags.Paused = 0;
				player_render_pause();
				player_audio_start();
			}
			else
			{
				player_flags.Paused = 1;
				player_render_pause();
				player_audio_stop();
			}
			break;

		case 1:
			/* Skip Forward */
			player_flags.SkipFwd = 1;
			break;

		case 2:
			/* Increase Skip Interval */
			if(player_skip_idx < PLAYER_NUM_INTERVALS - 1)
			{
				++player_skip_idx;
				player_render_interval();
			}
			break;

		case 3:
			player_flags.Rewind = 1;
			break;

		/* Row 2 */
		case 4:
			/* Exit */
			player_flags.Exit = 1;
			break;

		case 5:
			/* Skip Backward */
			player_flags.SkipBwd = 1;
			break;

		case 6:
			/* Decrease Skip Interval */
			if(player_skip_idx > 0)
			{
				--player_skip_idx;
				player_render_interval();
			}
			break;
	}
}

static void player_render_pause(void)
{
	lcd_cursor(0, 0);
	lcd_char(player_flags.Paused ? LCD_CHAR_PAUSE : LCD_CHAR_PLAY);
}

static void player_render_name(const char *name)
{
	const char *s;
	char c;
	lcd_cursor(2, 0);

	for(s = name; (c = *s); ++s)
	{
		if(c == '/')
		{
			name = s + 1;
		}
	}

	for(; (c = *name) && c != '.'; ++name)
	{
		lcd_char(c);
	}
}

static void player_render_plus_minus(void)
{
	lcd_cursor(12, 0);
	lcd_char(LCD_CHAR_PLUS_MINUS);
}

static void player_render_interval(void)
{
	const char *s = player_skip_interval_str + 3 * player_skip_idx;
	lcd_cursor(13, 0);
	lcd_char(pgm_read_byte(s));
	lcd_char(pgm_read_byte(s + 1));
	lcd_char(pgm_read_byte(s + 2));
}

static void player_render_timer(void)
{
	char buf[32];
	lcd_cursor(1, 1);
	snprintf(buf, sizeof(buf), "%1d:%02d:%02d/%1d:%02d:%02d",
			player_wi.ph, player_wi.pm, player_wi.ps,
			player_wi.lh, player_wi.lm, player_wi.ls);
	lcd_string(buf);
}

static void player_render_timer_partial(void)
{
	char buf[32];
	lcd_cursor(1, 1);
	snprintf(buf, sizeof(buf), "%1d:%02d:%02d",
			player_wi.ph, player_wi.pm, player_wi.ps);
	lcd_string(buf);
}

static void player_render_init(const char *name)
{
	uint32_t secs = player_wi.data_len / player_wi.sample_rate / player_wi.num_channels;

	player_wi.ph = 0;
	player_wi.pm = 0;
	player_wi.ps = 0;

	player_wi.lh = (secs / 3600) % 24;
	player_wi.lm = (secs / 60) % 60;
	player_wi.ls = secs % 60;

	lcd_clear();
	player_render_pause();
	player_render_name(name);
	player_render_plus_minus();
	player_render_interval();
	player_render_timer();
}

static void player_audio_play(const char *s)
{
	uint16_t n;
	uint32_t nread, secs;

rewind:
	n = 0;
	nread = 0;

	if(fat_fopen(s))
	{
		return;
	}

	if(player_wav_info())
	{
		return;
	}

	player_flags.Exit = 0;
	player_flags.Paused = 0;
	player_flags.Rewind = 0;
	player_flags.SkipBwd = 0;
	player_flags.SkipFwd = 0;

	player_read_offset = 0;
	player_write_offset = PLAYER_BUFFER_SIZE;

	nread = 0;
	player_buf_idx = player_wi.offset;
	OCR2A = player_wi.compare_value;
	player_audio_start();
	player_render_init(s);
	while(nread < player_wi.data_len)
	{
		button_check(player_event);
		if(player_flags.Exit)
		{
			goto exit;
		}

		if(player_flags.Rewind)
		{
			player_audio_stop();
			goto rewind;
		}

		if(player_flags.SkipFwd)
		{
			int32_t samples = nread + player_skip_amount[player_skip_idx] * player_wi.num_channels * player_wi.sample_rate;
			if(samples < player_wi.data_len)
			{
				player_audio_stop();
				nread = samples;
				fat_fseek(samples);
				player_audio_start();
			}

			player_flags.SkipFwd = 0;
		}

		if(player_flags.SkipBwd)
		{
			int32_t samples = nread - player_skip_amount[player_skip_idx] * player_wi.num_channels * player_wi.sample_rate;
			if(samples >= 0)
			{
				player_audio_stop();
				nread = samples;
				fat_fseek(samples);
				player_audio_start();
			}

			player_flags.SkipBwd = 0;
		}

		if(player_swap_flag)
		{
			if(fat_fread(player_buf + player_write_offset, PLAYER_BUFFER_SIZE, &n))
			{
				goto exit;
			}

			if(n < 512)
			{
				goto exit;
			}

			nread += n;
			player_swap_flag = 0;

			/* Update Timer */
			secs = nread / player_wi.sample_rate / player_wi.num_channels;
			player_wi.ph = (secs / 3600) % 24;
			player_wi.pm = (secs / 60) % 60;
			player_wi.ps = secs % 60;
			player_render_timer_partial();
		}
	}

exit:
	player_audio_stop();
}

static void player_isr_mono(void)
{
	uint8_t sample = player_buf[player_read_offset + player_buf_idx++];
	OCR0A = sample;
	OCR0B = sample;
	if(player_buf_idx == PLAYER_BUFFER_SIZE)
	{
		uint16_t tmp;
		player_buf_idx = 0;

		/* Swap */
		tmp = player_read_offset;
		player_read_offset = player_write_offset;
		player_write_offset = tmp;

		player_swap_flag = 1;
	}
}

static void player_isr_stereo(void)
{
	OCR0A = player_buf[player_read_offset + player_buf_idx++];
	OCR0B = player_buf[player_read_offset + player_buf_idx++];
	if(player_buf_idx == PLAYER_BUFFER_SIZE)
	{
		uint16_t tmp;
		player_buf_idx = 0;

		/* Swap */
		tmp = player_read_offset;
		player_read_offset = player_write_offset;
		player_write_offset = tmp;

		player_swap_flag = 1;
	}
}

/* Audio Interrupt */
ISR(TIMER2_COMPA_vect)
{
	if(player_wi.num_channels == 1)
	{
		player_isr_mono();
	}
	else
	{
		player_isr_stereo();
	}
}

