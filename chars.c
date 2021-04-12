/* LCD Custom Characters */
#define LCD_CHAR_PLAY       0
#define LCD_CHAR_PAUSE      1
#define LCD_CHAR_PLUS_MINUS 2

const uint8_t chr_play[] =
{
	0x08,
	0x0C,
	0x0E,
	0x0F,
	0x0E,
	0x0C,
	0x08,
	0x00
};

const uint8_t chr_pause[] =
{
	0x00,
	0x1B,
	0x1B,
	0x1B,
	0x1B,
	0x1B,
	0x00,
	0x00
};

const uint8_t chr_plus_minus[] =
{
	0x04,
	0x04,
	0x1F,
	0x04,
	0x04,
	0x00,
	0x1F,
	0x00
};

static void gchar_init(void)
{
	lcd_gchar(LCD_CHAR_PLAY, chr_play);
	lcd_gchar(LCD_CHAR_PAUSE, chr_pause);
	lcd_gchar(LCD_CHAR_PLUS_MINUS, chr_plus_minus);
}

