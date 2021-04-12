static uint32_t ld_u32(const uint8_t *p)
{
	return ((uint32_t)p[0]) | (((uint32_t)p[1]) << 8) |
		(((uint32_t)p[2]) << 16) | (((uint32_t)p[3]) << 24);
}

static uint16_t ld_u16(const uint8_t *p)
{
	return ((uint16_t)p[0]) | ((uint16_t)(p[1]) << 8);
}

static void mem_set(uint8_t *dst, uint8_t val, uint16_t cnt)
{
	while(cnt--)
	{
		*dst++ = val;
	}
}

static uint8_t mem_cmp
	(const uint8_t *dst, const uint8_t *src, uint16_t cnt)
{
	uint8_t r = 0;
	while(cnt-- && (r = *dst++ == *src++)) ;
	return r;
}

static const uint8_t *mem_mem(
	const uint8_t *haystack, uint16_t haystack_len,
	const uint8_t *needle, uint16_t needle_len)
{
	uint16_t i;

	if(needle_len == 0)
	{
		return haystack;
	}

	if(haystack_len < needle_len)
	{
		return 0;
	}

	for(i = 0; i <= haystack_len - needle_len; ++i)
	{
		if(mem_cmp(haystack + i, needle, needle_len))
		{
			return haystack + i;
		}
	}

	return 0;
}

