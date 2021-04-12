#define BUTTON_DEBOUNCE_TICKS 200

static const volatile uint8_t *_button_ports[7] = { &PINB, &PINC, &PINC, &PINC, &PINC, &PINC, &PINB };
static const uint8_t _button_pins[7] = { 1 << 0, 1 << 4, 1 << 3, 1 << 2, 1 << 1, 1 << 0, 1 << 1 };

static uint8_t _buttons[7];

static void button_init(void)
{
	PORTB |= (1 << PB0) | (1 << PB1);
	PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4);
}

static void button_check(void (*button_event)(uint8_t))
{
	uint8_t i;
	for(i = 0; i < 7; ++i)
	{
		if(!(*_button_ports[i] & _button_pins[i]))
		{
			if(_buttons[i] < BUTTON_DEBOUNCE_TICKS)
			{
				++_buttons[i];
			}
			else if(_buttons[i] == BUTTON_DEBOUNCE_TICKS)
			{
				_buttons[i] = BUTTON_DEBOUNCE_TICKS + 1;
				button_event(i);
			}
		}
		else
		{
			_buttons[i] = 0;
		}
	}
}
