typedef struct
{
	uint16_t index;
	uint8_t *fn;
	uint32_t sclust, clust, sect;
} dir_t;

typedef struct
{
	uint32_t size;
	uint8_t type;
	char name[13];
} direntry_t;

static uint32_t fat_fsize, fat_ftell;
#define fat_fsize() fat_fsize
#define fat_ftell() fat_ftell

static uint8_t fat_mount(void);
static uint8_t fat_fopen(const char *path);
static uint8_t fat_fread(void *buf, uint16_t btr, uint16_t *br);
static uint8_t fat_fseek(uint32_t offset);
static uint8_t fat_opendir(dir_t *dj, const char *path);
static uint8_t fat_readdir(dir_t *dj, direntry_t *fno);

#define BPB_SECTORS_PER_CLUSTER   13
#define BPB_RESERVED_SECTOR_COUNT 14
#define PBP_NUM_FATS              16
#define PBP_ROOT_ENTRY_COUNT      17
#define BPB_FAT_SIZE_16           22
#define BPB_FAT_SIZE_32           36
#define BPB_TOTAL_SECTORS_16      19
#define BPB_TOTAL_SECTORS_32      32
#define BPB_ROOT_CLUSTER          44
#define BS_FILESYSTEM_TYPE        54
#define BS_FILESYSTEM_TYPE_32     82
#define MBR_TABLE                446
#define DIR_NAME                   0
#define DIR_ATTR                  11
#define DIR_CLUSTER_HI            20
#define DIR_CLUSTER_LO            26
#define DIR_FILESIZE              28
#define AM_VOL                      0x08
#define AM_DIR                      0x10
#define AM_MASK                     0x3F

struct
{
	uint8_t csize;
	uint16_t n_rootdir;
	uint32_t n_fatent, fatbase, dirbase,
			database, org_clust, curr_clust, dsect;
} static _fs;

static uint32_t get_fat(uint32_t cluster)
{
	uint8_t buf[4];
	if(cluster < 2 || cluster >= _fs.n_fatent)
	{
		return 1;
	}

	if(sd_read(buf, _fs.fatbase + cluster / 128,
		((uint16_t)cluster % 128) * 4, 4))
	{
		return 1;
	}

	return ld_u32(buf) & 0x0FFFFFFF;
}

static uint32_t clust2sect(uint32_t cluster)
{
	cluster -= 2;
	if(cluster >= (_fs.n_fatent - 2))
	{
		return 0;
	}

	return cluster * _fs.csize + _fs.database;
}

static uint32_t get_cluster(uint8_t *dir)
{
	uint32_t cluster;
	cluster = ld_u16(dir + DIR_CLUSTER_HI);
	cluster <<= 16;
	cluster |= ld_u16(dir + DIR_CLUSTER_LO);
	return cluster;
}

static uint8_t create_name(dir_t *dj, const char **path)
{
	uint8_t c, ni, si, i, *sfn;
	const char *p;
	sfn = dj->fn;
	mem_set(sfn, ' ', 11);
	si = 0;
	i = 0;
	ni = 8;
	p = *path;
	for(;;)
	{
		c = p[si++];
		if(c <= ' ' || c == '/')
		{
			break;
		}

		if(c == '.' || i >= ni)
		{
			if(ni != 8 || c != '.')
			{
				break;
			}

			i = 8;
			ni = 11;
			continue;
		}

		/* Convert character to uppercase */
		if(c >= 'a' && c <= 'z')
		{
			c -= 'a' - 'A';
		}

		sfn[i++] = c;
	}

	*path = &p[si];
	sfn[11] = (c <= ' ');
	return 0;
}

static uint8_t dir_rewind(dir_t *dj)
{
	uint32_t cluster;
	dj->index = 0;
	cluster = dj->sclust;
	if(cluster == 1 || cluster >= _fs.n_fatent)
	{
		return 1;
	}

	if(!cluster)
	{
		cluster = (uint32_t)_fs.dirbase;
	}

	dj->clust = cluster;
	dj->sect = clust2sect(cluster);
	return 0;
}

static uint8_t dir_next(dir_t *dj)
{
	uint32_t clst;
	uint16_t i;
	if(!(i = dj->index + 1) || !dj->sect)
	{
		return 1;
	}

	if(!(i % 16))
	{
		dj->sect++;
		if(dj->clust == 0)
		{
			if(i >= _fs.n_rootdir)
			{
				return 1;
			}
		}
		else
		{
			if(((i / 16) & (_fs.csize - 1)) == 0)
			{
				clst = get_fat(dj->clust);
				if(clst <= 1)
				{
					return 1;
				}

				if(clst >= _fs.n_fatent)
				{
					return 1;
				}

				dj->clust = clst;
				dj->sect = clust2sect(clst);
			}
		}
	}

	dj->index = i;
	return 0;
}

static uint8_t dir_find(dir_t *dj, uint8_t *dir)
{
	uint8_t res;
	if((res = dir_rewind(dj)))
	{
		return res;
	}

	do
	{
		if((res = sd_read(dir, dj->sect, (dj->index % 16) * 32, 32)))
		{
			break;
		}

		if(dir[DIR_NAME] == 0)
		{
			res = 1;
			break;
		}

		if(!(dir[DIR_ATTR] & AM_VOL) && mem_cmp(dir, dj->fn, 11))
		{
			break;
		}

		res = dir_next(dj);
	} while(!res);
	return res;
}

static uint8_t follow_path(dir_t *dj, uint8_t *dir, const char *path)
{
	uint8_t res;
	while(*path == ' ')
	{
		++path;
	}

	if(*path == '/')
	{
		++path;
	}

	dj->sclust = 0;
	if(*path < ' ')
	{
		res = dir_rewind(dj);
		dir[0] = 0;
	}
	else
	{
		for(;;)
		{
			if((res = create_name(dj, &path)))
			{
				break;
			}

			if((res = dir_find(dj, dir)))
			{
				break;
			}

			if(dj->fn[11])
			{
				break;
			}

			if(!(dir[DIR_ATTR] & AM_DIR))
			{
				res = 1;
				break;
			}

			dj->sclust = get_cluster(dir);
		}
	}

	return res;
}

static uint8_t check_fs(uint8_t *buf, uint32_t sect)
{
	if(sd_read(buf, sect, 510, 2))
	{
		return 3;
	}

	if(ld_u16(buf) != 0xAA55)
	{
		return 2;
	}

	if(!sd_read(buf, sect, BS_FILESYSTEM_TYPE_32, 2)
		&& ld_u16(buf) == 0x4146)
	{
		return 0;
	}

	return 1;
}

static uint8_t fat_mount(void)
{
	uint8_t fmt, buf[36];
	uint32_t bsect, fsize, tsect, mclst;
	bsect = 0;
	if((fmt = check_fs(buf, bsect)))
	{
		if(!sd_read(buf, bsect, MBR_TABLE, 16))
		{
			if(buf[4])
			{
				bsect = ld_u32(&buf[8]);
				fmt = check_fs(buf, bsect);
			}
		}
	}

	if(fmt)
	{
		return 1;
	}

	if(sd_read(buf, bsect, 13, sizeof(buf)))
	{
		return 1;
	}

	if(!(fsize = ld_u16(buf + BPB_FAT_SIZE_16 - 13)))
	{
		fsize = ld_u32(buf + BPB_FAT_SIZE_32 - 13);
	}

	fsize *= buf[PBP_NUM_FATS - 13];
	_fs.fatbase = bsect + ld_u16(buf + BPB_RESERVED_SECTOR_COUNT - 13);
	_fs.csize = buf[BPB_SECTORS_PER_CLUSTER - 13];
	_fs.n_rootdir = ld_u16(buf + PBP_ROOT_ENTRY_COUNT - 13);
	if(!(tsect = ld_u16(buf + BPB_TOTAL_SECTORS_16 - 13)))
	{
		tsect = ld_u32(buf + BPB_TOTAL_SECTORS_32 - 13);
	}

	mclst = (tsect - ld_u16(buf + BPB_RESERVED_SECTOR_COUNT - 13) -
		fsize - _fs.n_rootdir / 16) / _fs.csize + 2;

	_fs.n_fatent = (uint32_t)mclst;
	if(!(mclst >= 0xFFF7))
	{
		return 1;
	}

	_fs.dirbase = ld_u32(buf + (BPB_ROOT_CLUSTER - 13));
	_fs.database = _fs.fatbase + fsize + _fs.n_rootdir / 16;
	return 0;
}

static uint8_t fat_fopen(const char *path)
{
	dir_t dj;
	uint8_t sp[12], dir[32];
	dj.fn = sp;

	if(follow_path(&dj, dir, path) || !dir[0] ||
		(dir[DIR_ATTR] & AM_DIR))
	{
		return 1;
	}

	_fs.org_clust = get_cluster(dir);
	fat_fsize = ld_u32(dir + DIR_FILESIZE);
	fat_ftell = 0;
	return 0;
}

static uint8_t fat_fread(void *buf, uint16_t btr, uint16_t *br)
{
	uint32_t clst, sect, remain;
	uint16_t rcnt;
	uint8_t cs, *rbuf;
	rbuf = buf;
	*br = 0;
	remain = fat_fsize - fat_ftell;
	if(btr > remain)
	{
		btr = (uint16_t)remain;
	}

	while(btr)
	{
		if((fat_ftell % 512) == 0)
		{
			if(!(cs = (uint8_t)(fat_ftell / 512 & (_fs.csize - 1))))
			{
				if((clst = fat_ftell ?
					get_fat(_fs.curr_clust) : _fs.org_clust) <= 1)
				{
					return 1;
				}

				_fs.curr_clust = clst;
			}

			if(!(sect = clust2sect(_fs.curr_clust)))
			{
				return 1;
			}

			_fs.dsect = sect + cs;
		}

		if((rcnt = 512 - (uint16_t)fat_ftell % 512) > btr)
		{
			rcnt = btr;
		}

		if(sd_read(rbuf, _fs.dsect, (uint16_t)fat_ftell % 512, rcnt))
		{
			return 1;
		}

		fat_ftell += rcnt;
		btr -= rcnt;
		*br += rcnt;
		if(rbuf)
		{
			rbuf += rcnt;
		}
	}

	return 0;
}

static uint8_t fat_fseek(uint32_t offset)
{
	uint32_t clst, bcs, sect, ifptr;
	if(offset > fat_fsize)
	{
		offset = fat_fsize;
	}

	ifptr = fat_ftell;
	fat_ftell = 0;
	if(offset > 0)
	{
		bcs = (uint32_t)_fs.csize * 512;
		if(ifptr > 0 && (offset - 1) / bcs >= (ifptr - 1) / bcs)
		{
			fat_ftell = (ifptr - 1) & ~(bcs - 1);
			offset -= fat_ftell;
			clst = _fs.curr_clust;
		}
		else
		{
			clst = _fs.org_clust;
			_fs.curr_clust = clst;
		}

		while(offset > bcs)
		{
			clst = get_fat(clst);
			if(clst <= 1 || clst >= _fs.n_fatent)
			{
				return 1;
			}

			_fs.curr_clust = clst;
			fat_ftell += bcs;
			offset -= bcs;
		}

		fat_ftell += offset;
		if(!(sect = clust2sect(clst)))
		{
			return 1;
		}

		_fs.dsect = sect + (fat_ftell / 512 & (_fs.csize - 1));
	}

	return 0;
}

/* Directory */
static void get_fileinfo(dir_t *dj, uint8_t *dir, direntry_t *fno)
{
	uint8_t i, c;
	char *p;
	p = fno->name;
	if(dj->sect)
	{
		for(i = 0; i < 8; i++)
		{
			c = dir[i];
			if(c == ' ')
			{
				break;
			}

			if(c == 0x05)
			{
				c = 0xE5;
			}

			*p++ = c;
		}

		if(dir[8] != ' ')
		{
			*p++ = '.';
			for(i = 8; i < 11; i++)
			{
				c = dir[i];
				if (c == ' ')
				{
					break;
				}

				*p++ = c;
			}
		}

		fno->type = dir[DIR_ATTR] & AM_DIR;
		fno->size = ld_u32(dir + DIR_FILESIZE);
	}

	*p = 0;
}

static uint8_t dir_read(dir_t *dj, uint8_t *dir)
{
	uint8_t res, a, c;
	res = 2;
	while(dj->sect)
	{
		if((res = sd_read(dir, dj->sect, (dj->index % 16) * 32, 32)))
		{
			break;
		}

		c = dir[DIR_NAME];
		if(!c)
		{
			res = 2;
			break;
		}

		a = dir[DIR_ATTR] & AM_MASK;
		if(c != 0xE5 && c != '.' && !(a & AM_VOL))
		{
			break;
		}

		if((res = dir_next(dj)))
		{
			break;
		}
	}

	if(res)
	{
		dj->sect = 0;
	}

	return res;
}

static uint8_t fat_opendir(dir_t *dj, const char *path)
{
	uint8_t res;
	uint8_t sp[12], dir[32];
	dj->fn = sp;
	if(!(res = follow_path(dj, dir, path)))
	{
		if(dir[0])
		{
			if(dir[DIR_ATTR] & AM_DIR)
			{
				dj->sclust = get_cluster(dir);
			}
			else
			{
				res = 2;
			}
		}

		if(!res)
		{
			res = dir_rewind(dj);
		}
	}

	return res;
}

static uint8_t fat_readdir(dir_t *dj, direntry_t *fno)
{
	uint8_t res, sp[12], dir[32];
	dj->fn = sp;
	if(!fno)
	{
		res = dir_rewind(dj);
	}
	else
	{
		if((res = dir_read(dj, dir)) == 2)
		{
			res = 0;
		}

		if(!res)
		{
			get_fileinfo(dj, dir, fno);
			if((res = dir_next(dj)) == 2)
			{
				res = 0;
			}
		}
	}

	return res;
}
