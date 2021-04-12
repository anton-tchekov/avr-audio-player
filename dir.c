#include <string.h>

/* [ VARIABLES ] */
static char dir_path[256] = "/";
static dir_t dir;
static direntry_t dirent[2];
static uint8_t dir_sel, dir_path_len = 0;

static void path_append(const char *s)
{
	char c;
	dir_path[dir_path_len++] = '/';
	for(; (c = *s); ++s)
	{
		dir_path[dir_path_len++] = c;
	}

	dir_path[dir_path_len] = '\0';
}

static void path_up(void)
{
	char *s;
	for(s = dir_path + dir_path_len; s >= dir_path; --s)
	{
		if(*s == '/')
		{
			break;
		}
	}

	*s = '\0';
	dir_path_len = s - dir_path;
}

static void dir_out_file(uint8_t line)
{
	char *s, c;
	lcd_cursor(2, line);
	s = dirent[line].name;
	for(; (c = *s) && c != '.'; ++s)
	{
		lcd_char(c);
	}

	lcd_cursor(13, line);
	if(dirent[line].type & AM_DIR)
	{
		lcd_string("DIR");
	}
	else
	{
		for(++s; (c = *s); ++s)
		{
			lcd_char(c);
		}
	}
}

static void dir_render(void)
{
	lcd_clear();

	/* Selected file */
	lcd_cursor(0, dir_sel & 1);
	lcd_char('>');

	/* Filenames */
	dir_out_file(0);
	if(dir_sel != 2)
	{
		dir_out_file(1);
	}
}

static void dir_prepare(void)
{
	dir_sel = 0;
	fat_opendir(&dir, dir_path);
	if(fat_readdir(&dir, &dirent[0]))
	{
		static const char dir_empty[] PROGMEM = "[ DIR EMPTY ]";
		lcd_clear();
		lcd_string_P(dir_empty);
		dir_sel = 3;
		return;
	}

	if(fat_readdir(&dir, &dirent[1]))
	{
		dir_sel = 2;
	}

	dir_render();
}

static void dir_event(uint8_t button)
{
	switch(button)
	{
		case 0:
			/* Up */
			if(dir_sel > 2)
			{
				return;
			}

			if(dir_sel == 1)
			{
				dir_sel = 0;
			}
			else if(dir_sel == 0)
			{
				uint8_t flag = 0;
				direntry_t de, prev;
				fat_opendir(&dir, dir_path);
				while(!fat_readdir(&dir, &de))
				{
					if(strcmp(de.name, dirent[0].name) == 0)
					{
						++flag;
						break;
					}

					memcpy(&prev, &de, sizeof(direntry_t));
					flag = 1;
				}

				if(flag == 2)
				{
					memcpy(&dirent[0], &prev, sizeof(direntry_t));
					memcpy(&dirent[1], &de, sizeof(direntry_t));
				}
			}

			dir_render();
			break;

		case 1:
			/* Enter */
			if(dir_sel > 2)
			{
				return;
			}

			{
				direntry_t *de = &dirent[dir_sel & 1];
				path_append(de->name);
				if(de->type & AM_DIR)
				{
					dir_prepare();
				}
				else
				{
					player_audio_play(dir_path);
					path_up();
					dir_render();
				}
			}
			break;

		case 2:
			/* Down */
			if(dir_sel > 2)
			{
				return;
			}

			if(dir_sel == 0)
			{
				dir_sel = 1;
			}
			else if(dir_sel == 1)
			{
				direntry_t de;
				if(fat_readdir(&dir, &de))
				{
					break;
				}

				memcpy(&dirent[0], &dirent[1], sizeof(direntry_t));
				memcpy(&dirent[1], &de, sizeof(direntry_t));
			}

			dir_render();
			break;

		case 3:
			/* Record mode */
			break;

		case 4:
		{
			/* Up Dir */
			uint8_t prev_len = dir_path_len;
			path_up();
			if(prev_len > dir_path_len)
			{
				dir_prepare();
			}
			break;
		}
	}
}

static void dir_open(void)
{
	dir_prepare();
	for(;;)
	{
		button_check(dir_event);
	}
}
