#define LCD_DELAY_US_ENABLE    20
#define LCD_DELAY_US_DATA      46
#define LCD_DELAY_US_COMMAND   42

#define LCD_DELAY_MS_BOOTUP    15
#define LCD_DELAY_MS_RESET_1    5
#define LCD_DELAY_MS_RESET_2    1
#define LCD_DELAY_MS_RESET_3    1
#define LCD_DELAY_MS_4BIT       5
#define LCD_DELAY_MS_HOME       2
#define LCD_DELAY_MS_CLEAR      2

/* Clear Display                 00000001 */
#define LCD_CLEAR_DISPLAY        0x01

/* Cursor Home                   0000001x */
#define LCD_CURSOR_HOME          0x02

/* Set Entry Mode                000001xx */
#define LCD_SET_ENTRY            0x04

#define LCD_ENTRY_DECREASE       0x00
#define LCD_ENTRY_INCREASE       0x02
#define LCD_ENTRY_NOSHIFT        0x00
#define LCD_ENTRY_SHIFT          0x01

/* Set Display                   00001xxx */
#define LCD_SET_DISPLAY          0x08

#define LCD_DISPLAY_OFF          0x00
#define LCD_DISPLAY_ON           0x04
#define LCD_CURSOR_OFF           0x00
#define LCD_CURSOR_ON            0x02
#define LCD_BLINKING_OFF         0x00
#define LCD_BLINKING_ON          0x01

/* Set Shift                     0001xxxx */
#define LCD_SET_SHIFT            0x10

#define LCD_CURSOR_MOVE          0x00
#define LCD_DISPLAY_SHIFT        0x08
#define LCD_SHIFT_LEFT           0x00
#define LCD_SHIFT_RIGHT          0x04

/* Set Function                  001xxxxx */
#define LCD_SET_FUNCTION         0x20

#define LCD_FUNCTION_4BIT        0x00
#define LCD_FUNCTION_2LINE       0x08
#define LCD_FUNCTION_5X7         0x00

#define LCD_SOFT_RESET           0x30

/* Set CGRAM Address             01xxxxxx (Character Generator RAM) */
#define LCD_SET_CGADR            0x40

/* Set DDRAM Address             1xxxxxxx (Display Data RAM) */
#define LCD_SET_DDADR            0x80

#define LCD_OFFSET_SECOND_ROW    0x40

#define LCD_DELAY_US(n)          _delay_us(n)
#define LCD_DELAY_MS(n)          _delay_ms(n)

#define LCD_OUT                  PORTD
#define LCD_DIR                  DDRD
#define LCD_RS                  7
#define LCD_EN                  4
#define LCD_DB                  0

#define LCD_WIDTH              16
#define LCD_HEIGHT              2

static void lcd_init(void);
static void lcd_data(uint8_t data);
static void lcd_command(uint8_t data);
static void lcd_clear(void);
static void lcd_string(const char *s);
static void lcd_string_P(const char *s);

#define lcd_char(c) \
	lcd_data(c)

#define lcd_cursor(x, y) \
	lcd_command(LCD_SET_DDADR + (x) + ((y) ? LCD_OFFSET_SECOND_ROW : 0))

static void lcd_enable(void)
{
	LCD_OUT |= (1 << LCD_EN);
	LCD_DELAY_US(LCD_DELAY_US_ENABLE);
	LCD_OUT &= ~(1 << LCD_EN);
}

static void lcd_out(uint8_t data)
{
	data &= 0xF0;
	LCD_OUT &= ~(0xF0 >> (4 - LCD_DB));
	LCD_OUT |= (data >> (4 - LCD_DB));
	lcd_enable();
}

static void lcd_init(void)
{
	uint8_t pins = (0x0F << LCD_DB) | (1 << LCD_RS) | (1 << LCD_EN);
	LCD_DIR |= pins;
	LCD_OUT &= ~pins;
	LCD_DELAY_MS(LCD_DELAY_MS_BOOTUP);
	lcd_out(LCD_SOFT_RESET);
	LCD_DELAY_MS(LCD_DELAY_MS_RESET_1);
	lcd_enable();
	LCD_DELAY_MS(LCD_DELAY_MS_RESET_2);
	lcd_enable();
	LCD_DELAY_MS(LCD_DELAY_MS_RESET_3);
	lcd_out(LCD_SET_FUNCTION | LCD_FUNCTION_4BIT);
	LCD_DELAY_MS(LCD_DELAY_MS_4BIT);
	lcd_command(LCD_SET_FUNCTION | LCD_FUNCTION_4BIT |
		LCD_FUNCTION_2LINE | LCD_FUNCTION_5X7);
	lcd_command(LCD_SET_DISPLAY | LCD_DISPLAY_ON |
		LCD_CURSOR_OFF | LCD_BLINKING_OFF);
	lcd_command(LCD_SET_ENTRY | LCD_ENTRY_INCREASE |
		LCD_ENTRY_NOSHIFT);
	lcd_clear();
}

static void lcd_data(uint8_t data)
{
	LCD_OUT |= (1 << LCD_RS);
	lcd_out(data);
	lcd_out(data << 4);
	LCD_DELAY_US(LCD_DELAY_US_DATA);
}

static void lcd_command(uint8_t data)
{
	LCD_OUT &= ~(1 << LCD_RS);
	lcd_out(data);
	lcd_out(data << 4);
	LCD_DELAY_US(LCD_DELAY_US_COMMAND);
}

static void lcd_clear(void)
{
	lcd_command(LCD_CLEAR_DISPLAY);
	LCD_DELAY_MS(LCD_DELAY_MS_CLEAR);
}

static void lcd_string(const char *s)
{
	register uint8_t c;
	for(; (c = (uint8_t)*s); ++s)
	{
		lcd_data(c);
	}
}

static void lcd_string_P(const char *s)
{
	register uint8_t c;
	for(; (c = (uint8_t)pgm_read_byte(s)); ++s)
	{
		lcd_data(c);
	}
}

static void lcd_gchar(uint8_t addr, const uint8_t *data)
{
	uint8_t i;
	lcd_command(LCD_SET_CGADR | (addr << 3));
	for(i = 0; i < 8; ++i)
	{
		lcd_data(data[i]);
	}

	lcd_command(LCD_SET_DDADR);
}

