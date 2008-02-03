// extract-md by Michael Steil <mist@c64.org>
//
// extracts text and graphics from all Magic Disk 64 issues
// due to the TIMEX copy protection, issues 04/90 to 05/91
// lack the names of the texts

// TODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODO
// XXX: circles in files
// XXX: crosslinked files?
// 3x bus error, 1x segmentation fault
// 2x decompression failed
//  MD9303-UTILITIES-MUSIX MIXER.html -- file BROKEN!!
//  MD9112-INTERN-HOTLINE.html        -- file BROKEN!! (graphics; text is ok)
// MD9108, text "91" is missing
// MD9503, a lot missing
// check umlauts. scan for Üsterreich
// extract music as well?
// make sure all files are OK (and are text!)
// TODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODOTODO

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BLOCKS 683
#define BLOCKSIZE 256
#define BLOCK(n) (d64buf+BLOCKSIZE*n)

#define LINK_TRACK(i) (BLOCK(i)[0])
#define LINK_SECTOR(i) (BLOCK(i)[1])

unsigned char d64buf[BLOCKSIZE * BLOCKS];

int linear(int track, int sector) {
	track--;
	if (track<17)
			return sector + track*21;
	if (track<24)
			return sector + (track-17)*19 + 17*21;
	if (track<30)
			return sector + (track-24)*18 + (24-17)*19 + 17*21;
	return sector + (track-30)*17 + (30-24)*18 + (24-17)*19 + 17*21;
}

int unlinear(int l, int *t, int *s) {
	if (l<17*21) {
		*t = l/21 + 1;
		*s = l % 21;
		return;
	}
	l -= 17*21;
	if (l<(24-17)*19) {
		*t = l/19 + 17 + 1;
		*s = l % 19;
		return;
	}
	l -= (24-17)*19;
	if (l<(30-24)*18) {
		*t = l/18 + 24 + 1;
		*s = l % 18;
		return;
	}
	l -= (30-24)*18;
	*t = l/17 + 30 + 1;
	*s = l % 17;
}

int prev(int l) {
	int i;
	
	for (i=0; i<BLOCKS; i++) {
		if (l==linear(LINK_TRACK(i),LINK_SECTOR(i))) {
			/* more than one block could link here -
			   these would be crosslinked files;
			   we just ignore these for now */
			return i;
		}
	}
	return -1; /* not found */
}

int follow(int l, unsigned char *file) {
	int count = 0;
	char fn[8];
	FILE *f;
	unsigned char *p = file;

//	sprintf(fn, "%i.prg", l);
//	printf("%s", fn);
//	f = fopen(fn, "w");
	
//	printf("follow:\n");
	do {
		count++;
//		printf("%i \n", l);
		if (linear(LINK_TRACK(l),LINK_SECTOR(l))==l)
			break; /* loop!*/
		if (LINK_TRACK(l) == 0) {
			memcpy(p, BLOCK(l)+2, LINK_SECTOR(l)-1);
			p+=LINK_SECTOR(l)-1;
//			printf("(%i BLOCKS)\n", count);
			break;
		} else if (LINK_TRACK(l)>35) {
			memcpy(p, BLOCK(l), BLOCKSIZE);
			printf("ILLEGAL TRACK %i\n", LINK_TRACK(l));
			return -1;
		} else { /* we should check here whether the sector number is legal */
			memcpy(p, BLOCK(l)+2, BLOCKSIZE-2);
			p+=BLOCKSIZE-2;
			l = linear(LINK_TRACK(l), LINK_SECTOR(l));
		}
	} while(1);
//	fclose(f);
	return p - file;
}

#define BYTE(a) (buf[a])
#define WORD(a) (buf[a] | (buf[a+1]<<8))
#define LO(a) (a & 255)
#define HI(a) (a >> 8)

/* number of pages that have already been written
   = how much previous data we can reference */
unsigned char  PAGECNT_X25;

unsigned short LEN_X2728;

int SSRC_X292A;
int BSRC_X2B2C; /* must be signed and > 16 bits so that we can detect underflows */
unsigned short DST_X2D2E;

unsigned char PAGECNT_MAX;
unsigned short startaddress;

unsigned int textlength;

void dump(unsigned char* buf, int len) {
	int i, j;
	for (i=0; i<len; i+=16) {
		printf("%04x: ", i);
		for (j=0; j<16; j++) {
			printf("%02x ", BYTE(i+j));
		}
		printf("\n");
	}
}

void dumpscr(unsigned char* buf, int len) {
	int i, j;
	unsigned char c;
	for (i=0; i<len; i+=40) {
		printf("%04x: ", i);
		for (j=0; j<40; j++) {
			c = BYTE(i+j);
			if (c<0x20) c+=0x60;
			printf("%c", c);
		}
		printf("\n");
	}
}

#define END_OF_STREAM 0x100

/* returns character or END_OF_STREAM if end of stream */
unsigned int CHRGET(unsigned char *buf) {
	if (BSRC_X2B2C<=startaddress) {
		if (DST_X2D2E!=BSRC_X2B2C &&
		    ((unsigned char)DST_X2D2E-1)!=((unsigned char)BSRC_X2B2C)) { /* this line is a hack... necessary for C1 MDs */
			printf("Warning: decompression failed! %x/%x\n", (unsigned char)DST_X2D2E, (unsigned char)BSRC_X2B2C);
		}
		return END_OF_STREAM;
	}
	BSRC_X2B2C--;
//	printf("BSRC_X2B2C = %04x\n", BSRC_X2B2C);
//	printf("GET: %02x\n", BYTE(BSRC_X2B2C));
	return BYTE(BSRC_X2B2C);
}

/* it is important to copy backwards! */
void my_memcpy(unsigned char *buf, unsigned short dest, unsigned short src, int len) {
	int i;

//printf("COPY %04x->%04x (%02x)\n", src, dest, len);
	for (i=len-1; i>=0; i--) {
		BYTE(dest+i) = BYTE(src+i);
	}
}

void COPY(unsigned char *buf, int *src) {
	int i;
//unsigned char temp[1000];
	int dec, a, b;

	/* count pages */
	a = HI(DST_X2D2E);
	b = HI(DST_X2D2E-LEN_X2728);
	dec = HI(DST_X2D2E) - HI(DST_X2D2E-LEN_X2728);
	for (i=0; i<dec; i++)
		/* count up PAGECNT_X25 until PAGECNT_MAX reached */
		if (PAGECNT_X25<PAGECNT_MAX)
			PAGECNT_X25++;

	DST_X2D2E -= LEN_X2728;
	*src -= LEN_X2728;

	my_memcpy(buf, DST_X2D2E, *src, LEN_X2728);

/*
	printf("\"");
	for (i=0; i<LEN_X2728; i++) {
		char c = BYTE(*src+i);
		if (c<0x20) c+=0x60;
		printf("%c", c);
	}
	printf("\" (%i chars)\n", LEN_X2728);
*/
}

/*
 * method = 0: boot data
 * method = 1: runtime text/graphics
 */
int decompress(unsigned char *buf, int method, unsigned char **buf2) {
	unsigned char A;
	unsigned char CODE_X24;
	unsigned char NEXTLEN_X26;
	int c;
/*
start+5 = F0 +2
start+6 = F1 +3
2B/2C   = F2/F3 +4/5
2D/2E   = F4/F5 +6/7
24      = F7    +9
25      = F8    +10
26      = F9    +11
29/2A   = FA/FB +12/13
27/28   = FC/FD +14/15
*/

//	dump(buf, 16);

	if (!method) { /* boot */
		unsigned short metadata = WORD(0);
		printf("metadata = %04x\n", metadata);
		BYTE(0) = BYTE(metadata);   /* restore bytes 0 and 1 that have been */
		BYTE(1) = BYTE(metadata+1); /* overwritten with pointer to metadata */
//		printf("0 = %02x\n", BYTE(0));
//		printf("1 = %02x\n", BYTE(1));
		LEN_X2728 = BYTE(metadata+2);
		PAGECNT_MAX = BYTE(metadata+3);
		BSRC_X2B2C = WORD(metadata+4);
		DST_X2D2E = WORD(metadata+6);
		textlength = DST_X2D2E;
		startaddress = 0;
	} else { /* text/graphics */
		BSRC_X2B2C = WORD(1) - 0x800;
		DST_X2D2E = WORD(3) - 0x800;
		LEN_X2728 = BYTE(5);
		PAGECNT_MAX = BYTE(6);
		startaddress = 0;
		textlength = DST_X2D2E - (startaddress);
		buf += 8;
	}
	PAGECNT_X25 = 0; /* we can reference 0 pages */

#if 0
	printf("BSRC_X2B2C   = %04x\n", BSRC_X2B2C);
	printf("DST_X2D2E    = %04x\n", DST_X2D2E);
	printf("PAGECNT_MAX  = %02x\n", PAGECNT_MAX);
	printf("LEN_X2728    = %04x\n", LEN_X2728);
	printf("textlength   = %04x\n", textlength);
	printf("startaddress = %04x\n", startaddress);
#endif

	/* If the file is a picture, then there compressed text at the end.
	   In this case, tell the caller about it */
	if (buf2) {
		if (DST_X2D2E==0x2711 || /* 0x2711 = length of a KOALA pic */
			DST_X2D2E==0x2712 || /* for some reason, C1 MDs have KOALAs */
			DST_X2D2E==0x2720 || /* of strange lengths */
			DST_X2D2E==0x277F) {
			/* we must move the text out of the way, so that decompressing
			   the picture does not overwrite it */
			memmove(&buf[DST_X2D2E], &buf[BSRC_X2B2C], 32*1024);
			*buf2 = buf+DST_X2D2E;
		} else {
			*buf2 = 0;
		}
	}
//	dump(&buf[BSRC_X2B2C], 128);
	
	while(1) {
		if (LEN_X2728) {
//printf("VERBATIM ");
			COPY(buf, &BSRC_X2B2C);
		}

		c = CHRGET(buf); if(c==END_OF_STREAM) break;
		CODE_X24 = c;
//printf("CODE_X24 = %02x\n", CODE_X24);
		if (!CODE_X24) {
			/* copy 256 bytes from BSRC to DST, then get next code */
//printf("256 bytes verbatim\n");
			LEN_X2728 = 0x100;
			continue;
		}
		
		SSRC_X292A = NEXTLEN_X26 = CODE_X24 & 0x3F;
	//	printf("B SSRC_X292A = %04x\n", SSRC_X292A);
	//	printf("NEXTLEN_X26   = %02x\n", NEXTLEN_X26);

//printf("DST_X2D2E = %04x\n", DST_X2D2E);
		
		if (!(CODE_X24 & 0x80)) {; /* 0xxxxxxx */
			if (!(CODE_X24 & 0x40)) { /* 00xxxxxx */
//printf("00\n");
				NEXTLEN_X26 = 0; /* no next copy from BSRC */
				LEN_X2728 = 2; /* copy 2 bytes this time */
			} else {	/* 01xxxxxx */
//printf("01\n");
				c = CHRGET(buf); if(c==END_OF_STREAM) break;
				SSRC_X292A = c; /* get displacement */
//printf("C REL = %04x\n", SSRC_X292A);
				LEN_X2728 = 3; /* copy 3 bytes this time */
			}
		} else { /* 1xxxxxxx */
			if (!(CODE_X24 & 0x40)) { /* 10xxxxxx */
//printf("10 NEXTLEN = %04x\n", NEXTLEN_X26);
				LEN_X2728 = 4; /* copy 4 bytes this time */
				if (NEXTLEN_X26==0x3f) { /* all bits set in input -> ESC */
//printf("A\n");
					c = CHRGET(buf); if(c==END_OF_STREAM) break;
					NEXTLEN_X26 = c; /* get full 8 bits */
				}
				/* next copy from BSRC: either up to 63 bytes directly
				   encoded in code, or up to 255, encoded in following code.
				   note: may be 0 */
			} else {	/* 11xxxxxx */
//printf("11\n");
				if (NEXTLEN_X26>=5) {
					/* >= 5 -> this is the effective length */
					LEN_X2728 = NEXTLEN_X26;
				} else {
					/* < 5 -> this is hi byte, get low from code */
					c = CHRGET(buf); if(c==END_OF_STREAM) break;
					LEN_X2728 = c | NEXTLEN_X26 << 8;
				}	
				c = CHRGET(buf); if(c==END_OF_STREAM) break;
				NEXTLEN_X26 = c; /* get full 8 bits */
			}

			c = CHRGET(buf); if(c==END_OF_STREAM) break;
			A = c; /* get 8 bits */
//printf("B1 = %04x\n", A);
			if (A > PAGECNT_X25) {
				SSRC_X292A = A - PAGECNT_X25;
//printf("A REL = %04x\n", SSRC_X292A);
//printf("PAGECNT_X25 = %04x\n", PAGECNT_X25);
			} else {
				c = CHRGET(buf); if(c==END_OF_STREAM) break;
				SSRC_X292A = A << 8 | c;
//printf("B REL = %04x\n", SSRC_X292A);
			}
		}

		SSRC_X292A += DST_X2D2E; /* convert relative to absolute */
//printf("LZ ");
		COPY(buf, &SSRC_X292A);
		LEN_X2728 = NEXTLEN_X26;
	}
	return textlength;
}

#define LEVEL1MAX 10
#define LEVEL2MAX 10
#define MAXENTRYLEN (64)
struct {
	int track;
	int sector;
} textfile[LEVEL1MAX][LEVEL2MAX];

char* screen2htmlchar(unsigned char c) {
	switch (c) {
	case '@':
		return "-";
	case 'm':
		return "\\";
	case 'n':
		return "\\";
	case 'p':
		return "/";
	case 'q':
		return "-"; /* should be an inverted 'T' symbol */
	case 'r':
		return "-"; /* should be a 'T' symbol */
	case ']':
		return "|";
	case '}':
		return "/";
	case 0x94:
		return "&auml;";
	case 0x95:
		return "&ouml;";
	case 0x96:
		return "&uuml;";
	case 0x97:
		return "&szlig;";
	case 0x98:
		return "&Auml;";
	case 0x99:
		return "&Uuml;"; // sic!
	case 0x9A:
		return "&Uuml;"; // sic! (sometimes Ouml...)
	case '<':
		return "&lt;";
	case '>':
		return "&gt;";
	default:
		return NULL;
	}
}

void screen2html(unsigned char* fn, unsigned char* file, int len) {
	FILE *f;
	unsigned char fn2[MAXENTRYLEN];
	int i;
	char *s;
	unsigned char c;

	sprintf((char*)fn2, "%s.html", fn);
	f = fopen((char*)fn2, "w");
//	fputc(0, f);
//	fputc(8, f);
//	fwrite(file, 1, len, f);

	fprintf(f, "<pre>");
	for (i=0; i<len; i++) {
		if ((i % 40) == 0)
			fprintf(f, "\n");
		c = file[i];
		s = screen2htmlchar(c);
//		printf("%x %x\n", c, s);
		if (s) {
			fprintf(f, s);
		} else {
			if (!c) {
				c = ' ';
			} else if (c<0x20) {
				c += 0x60;
			}
			fputc(c, f);
//		printf("'%c'\n", c);
		}
	}
	fprintf(f, "</pre>");

	fclose(f);
}


void extract(unsigned char *d64buf, unsigned char *fn, int t, int s) {
	unsigned char file[BLOCKS*BLOCKSIZE];
	unsigned char *file2, *textfile;
	unsigned char fn2[MAXENTRYLEN];
	int len;

	bzero(file, BLOCKS*BLOCKSIZE);

//	if (t!=28 || s!=2) return;

	len = follow(linear(t, s), file);
	printf("start: %x\n", file[0] | file[1]<<8);
	if (((file[0] | file[1]<<8) != 0x7f8)
	 &&((file[0] | file[1]<<8) != 0x17f8)) { /* some texts in MD9106, MD9108 have 0x17f8 */
//		printf("no compressed file.\n");
		return;
	}

//	dump(file, len);
	len = decompress(&file[2], 1, &file2);
	printf("%i,%i %s (%i bytes)\n", t, s, fn, len);

#if 0
	sprintf((char*)fn2, "%s.prg", fn);
	FILE *f = fopen((char*)fn2, "w");
	fputc(0, f);
	fputc(0x08, f);
	fwrite(file+10, 1, len, f); /* skip old load address + 8 management bytes */
	fclose(f);
#endif

	if (file[10]==0x10 && file[11]==0x00) {
//		printf("File is GAME ON advertisement.\n");
		return;
	}

//	printf("%x\n", file);
//	printf("%x\n", file2);
//	if (file2) dump(file2, 1000);

	if (file2) {
		printf("KOALA found\n");
		sprintf((char*)fn2, "%s.koala", fn);
		FILE *f = fopen((char*)fn2, "w");
		fputc(0, f);
		fputc(0x60, f);
		fwrite(file+10, 1, len, f); /* skip old load address + 8 management bytes */
		fclose(f);

		len = decompress(file2, 1, 0);
		printf("%i,%i %s (%i bytes)\n", t, s, fn, len);
		textfile = file2-2;
	} else {
		textfile = file;
	}
//	printf("%x\n", textfile);

	screen2html(fn, textfile+10, len);
}

unsigned char menuentry[LEVEL1MAX][LEVEL2MAX][MAXENTRYLEN];

int load_boot(unsigned char* d64buf, unsigned char *file) {
	int i, t, s, len;
//	dump(&d64buf[(linear(18,1)<<8)], 256);

// there must be a file named "BOOT" on 18/01
// XXX maybe it's always at pos 0...
	int found = 0;
	for (i=0; i<8; i++) {
		if (!strncmp("BOOT", (char*)BLOCK(linear(18,1))+i*32+5, 4)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		printf("Could not find \"BOOT\"!\n");
		return 1;
	}
	t = *(BLOCK(linear(18,1))+i*32+3);
	s = *(BLOCK(linear(18,1))+i*32+4);
//	printf("\"BOOT\" found at %i, linking to %i/%i\n", i, t, s);
	
	// extract "BOOT"
	len = follow(linear(t,s), file);
//	printf("\"BOOT\" is %i bytes\n", len);

#if 0
	// check BOOT for consistency
	if (file[0]!=2 || file[1]!=1              /* load address */
	 || file[3]!=0xA9 || file[13]!=0xA9       /* LDAs for load address */
	 || file[0x8D]!=0xC9 || file[0x93]!=0xC9  /* CMPs for max address */
	 || file[0xB2]!=0xA9 || file[0xB7]!=0xA9) /* LDA for track/sector */
	{
		printf("\"BOOT\" file not recognized!");
		return 2;
	}
	loadaddr = file[4] | file[14] << 8;
	maxaddr = file[0x8E] | file[0x94] << 8;
//	printf("\"MAIN\" load address 0x%04x\n", loadaddr);
//	printf("\"MAIN\" max  address 0x%04x\n", maxaddr);
#endif
	return len;
}

int extract_md_C2b(int month, int year) {
	int i, j;
	int t, s;
	int p;
	unsigned char file[BLOCKS*BLOCKSIZE];
	unsigned char *file2, *file3, *file4;
	int len;
	unsigned short loadaddr, maxaddr;
	int offset, dirindex;
	
	len = load_boot(d64buf, file);
	if (len==252) { /* C1a */
		loadaddr = 0xb67f;
		maxaddr = 0xcd68;
		t = file[0xB3];
		s = file[0xB8];
		offset = 0x11;
		dirindex = 0x69e3;
	} else if (len==356) { /* C1b */
		loadaddr = 0xb6b9;
		maxaddr = 0xcd68;
		t = file[0x60];
		s = file[0x65];
		offset = 0;
		if (month==9 && year==91) /* for some reason, 09/91 has the new memory layout */
			dirindex = 0x69e3;    /* and 10/91 does not - 11/91 is C1b then */
		else 
			dirindex = 0x5898;
	} else {
		printf("\"BOOT\" file not recognized!\n");
		return 2;
	}
	printf("\"MAIN\" at %i/%i\n", t, s);

// load and decode MAIN
	len = follow(linear(t,s), file);
	printf("\"MAIN\" is %i bytes\n", len);

	file2 = &file[maxaddr-loadaddr+offset];
//	dump(file2, len-(maxaddr-loadaddr));
//	printf("%x\n", file2[0]);
//	printf("%x\n", file2[1]);
//	printf("len of part %x\n", len-(maxaddr-loadaddr));
	
	len = decompress(file2, 0, 0);
//	printf("decompressed len of part %x\n", len);

//		printf("DST_X2D2E = %04x\n", DST_X2D2E);
//		printf("BSRC_X2B2C = %04x\n", BSRC_X2B2C);

#if 1
	FILE *f = fopen("main.prg", "w");
//	fputc(0, f);
//	fputc(0x08, f);
	fwrite(file2, 1, 0xa8fd, f);
	fclose(f);
#endif

	for (i=0; i<LEVEL1MAX; i++) {
		for (j=0; j<LEVEL2MAX; j++) {
			textfile[i][j].track = 0;
		}
	}

// find exact dirindex - C1a MDs had the dirindex within a region
	for (i=-16; i<16; i++) {
		if (file2[dirindex+i]=='1' && file2[dirindex+i+1]=='1') {
			dirindex = dirindex + i - 2;
			break;
		}
	}

	unsigned char *dir = &file2[dirindex];
	int k = 0;
	while(dir[k]!=0xff) {
		/* fix: MD9201 defines a file in category 8 that doesn't exist */
		if (year==92 && month==1 && dir[k+2]=='8') break;

		printf("\"%c%c\" %i,%i\n", dir[k+2], dir[k+3], dir[k+0], dir[k+1]);
		int index1 = dir[k+2]-'0'-1;
		int index2 = dir[k+3]-'0'-1;
		textfile[index1][index2].track = dir[k+0];
		textfile[index1][index2] .sector = dir[k+1];
		k+=4;
	}
	file3 = &file2[0x0900];
//	printf("%x\n", file3[0]);
//	printf("%x\n", file3[1]);
//	printf("%x\n", file3[2]);
//	printf("%x\n", file3[3]);
	
	len = decompress(file3, 1, 0);
//	printf("decompressed len of menu %x\n", len);
//	dump(file3, len);
	
//	FILE *f2 = fopen ("del.prg", "w");
//	fputc(0, f2);
//	fputc(8, f2);
//	fwrite(file3, 1, len, f2);
//	fclose(f2);

#define MENUPTR(n) (file3[17+n*2] | file3[17+n*2+1] << 8)
#define MENUITEMS(n) file3[0x24+n]

	file4 = &file3[8+0x100];
	int l, m, k2, charcount, leadin, charfound, menubase;
	char cc;
	for (m=0; m<9; m++) {
		k2 = 0;
		menubase = MENUPTR(m)-0x0900;
//		printf("%x ", 0x900+menubase);
		for (k=0; k<11; k++) {
			leadin = 1;
			charfound = 0;
			for (l=0; l<16; l++) {
				cc = file4[menubase+k*16+l];
				if (cc !=0 && cc!=' ')
					charfound = 1;
			}
			if (!charfound) continue;
//			if (file4[menubase+k*16+2]=='-')
//				break;
			if (k>=MENUITEMS(k))
				break;

			if (m>0) {
				/* there are dead entries in almost every MAGIC DISK -
				   we sort them out if there is no track/sector pair
				   defined for the menu entry */
//				printf("%i\n", textfile[m-1][k2].track);
				if (!textfile[m-1][k2].track)
					continue;
//				printf(" (%i,%i)", m-1, k2);
//				printf("%i,%i ", textfile[m-1][k2].track, textfile[m-1][k2].sector);
				/* prepend level1 title */
//				printf("%s/", menuentry[0][m-1]);

//				strcpy((char*)menuentry[m][k2], (char*)menuentry[0][m-1]);
//				int ll = strlen((char*)menuentry[m][k2]);
//				menuentry[m][k2][ll]='-';
//				menuentry[m][k2][ll+1]='0';
//				charcount = ll+1;
				sprintf((char*)menuentry[m][k2], "MD%02i%02i-%s-", year, month, menuentry[0][m-1]);
				charcount = strlen((char*)menuentry[m][k2]);
			} else {
				charcount = 0;
			}
			for (l=0; l<16; l++) {
				cc = file4[menubase+k*16+l];
				if (!cc) cc = ' ';
				if (leadin) {
					if (cc!=' ')
						leadin = 0;
				}
				/* Some early issues (91/11, 91/12, 92/01, 92/02, 92/04) had "Tips & Tricks"
				   in mixed upper/lower case. As this was the only item in category
				   "HELP", the text was never shown - but we have to fix it
				   and convert it to uppercase */
				if (cc<0x20) cc+=0x40;
//				printf("%i %i %i | ", m, k2, charcount);
				if (!leadin) {
					menuentry[m][k2][charcount] = cc;
//					if (m>0)
//						printf("%c", cc);
					charcount++;
				}
			}
			menuentry[m][k2][charcount] = 0;
//			printf("\n");
//			dump(menuentry[m][k2], 32);
			if (m>0)
				extract(d64buf, menuentry[m][k2], textfile[m-1][k2].track, textfile[m-1][k2].sector);
//				printf("%s\n", menuentry[m][k2]);
//				printf("\n");
			k2++;
		}
//		printf("----------------\n");
	}

	return 0;
}

int extract_md_C2a(int month, int year) {

}

int extract_md_C1b(int month, int year) {
	/*
	 * TIMEX is a very nasty copy protection scheme - there is no way to
	 * decode the main program without writing a complete C64 emulator,
	 * so simply extract all files (without a directory entry) that look
	 * like text - unfortunately we don't have the name of the text then.
	 */
	int i, p, t, s;
	unsigned char fn[MAXENTRYLEN];

	for (i=0; i<BLOCKS; i++) {
		p = prev(i);
		unlinear(i, &t, &s);
		sprintf((char*)fn, "MD%02i%02i-%03i", year, month, i);
		/*
		 * no block links to the current one: it's a start block
		 * also make sure the link is correct - otherwise it's
		 * just an unused block
		 */
		if (p<0 && LINK_TRACK(i)<=35) {
//			printf("block %i (prev: %i) ((t=0x%02x s=0x%02x))\n", i, p, t, s);
			extract(d64buf, fn, t, s);
		}
	}
	 
	 return 0;
}

int extract_md_C1a(int month, int year) {
	int dir, i;
	unsigned char fn[MAXENTRYLEN];
	/*
	 * The C1a issues are identical with the C1b ones, but they have all
	 * texts visible as files in the directory. The filenames are of the
	 * form "ab", where "a" is a digit representing the category, and
	 * "b" a digit representing the number within that category.
	 * extract_md_C1b() would work with C1a as well, but we would lose
	 * the categories.
	 */
	 
	dir = linear(18,1);
	while(1) {
		for (i=0; i<8; i++) { /* dir entries */
#define DENTRY(a,b) (BLOCK(a)+b*32)
#define ISHEX(a) ((a>='0' && a<='9')||(a>='A' && a<='F'))
			if (DENTRY(dir, i)[2] &&  /* not deleted */
			    ISHEX(DENTRY(dir, i)[5]) && 
			    ISHEX(DENTRY(dir, i)[6]) && 
			    DENTRY(dir, i)[7]==0xA0) {
				printf("%c%c\n", DENTRY(dir, i)[5], DENTRY(dir, i)[6]);
				sprintf((char*)fn, "MD%02i%02i-%c%c", year, month, DENTRY(dir, i)[5], DENTRY(dir, i)[6]);
				extract(d64buf, fn, DENTRY(dir, i)[3], DENTRY(dir, i)[4]);
			}
		}
		if (!LINK_TRACK(dir)) break;
		dir = linear(LINK_TRACK(dir),LINK_SECTOR(dir));
	}
}

int get_issue(int *m, int *y) {
	int month, year;

#define BAM (linear(18,0)<<8)
#define DISKNAME (BAM+0x90)
// print disk name
	if (strncmp("MAGIC", (char*) &d64buf[DISKNAME], 5)) {
		printf("This is no supported MAGIC DISK!\n");
		return 1;
	}

/*
	int i;
	for (i=0; i<16; i++) {
		printf("%c", d64buf[DISKNAME+i]);
	}
	printf("\n");
*/

// find out issue
// XXX this code does not correctly detect all MDs yet!
// XXX also use checksum to verify it is a perfect copy
	if (!strncmp("04+05", (char*) &d64buf[DISKNAME+11], 5)) {
		month = 4;
		year = 92;
//		printf("04-05/92\n");
	} else if (!strncmp("4+5/9", (char*) &d64buf[DISKNAME+11], 5)) {
		month = 4;
		year = 93;
//		printf("04-05/93\n");
	} else if (!strncmp("07+08", (char*) &d64buf[DISKNAME+11], 5)) {
		if (d64buf[(linear(18,1)<<8)]==0) { /* 07-08/93 has 2 directory blocks */ 
			month = 7;
			year = 92;
//			printf("07-08/92\n");
		} else {
			month = 7;
			year = 93;
//			printf("07-08/93\n");
		}
	} else {
		month = (d64buf[DISKNAME+11]-'0')*10 + d64buf[DISKNAME+12]-'0';
		year = (d64buf[DISKNAME+14]-'0')*10 + d64buf[DISKNAME+15]-'0';
	}
	*m = month;
	*y = year;
	return 0;
}

int main(int argc, char **argv) {
	FILE *f;
	int error;

//	int t;
//	for (t=1; t<=36; t++) {
//		printf("%i %i\n", t, linear(t, 0));
//	}
//	return 0;

	if (argc!=2) {
		printf("extract-md\nUsage: diskmag md.d64\n");
		exit(1);
	}
		
	f = fopen(argv[1], "r");
	fread(d64buf, 1, sizeof(d64buf), f);
	fclose(f);

//	extract(d64buf, "del", 28, 2);
//	extract(d64buf, "del2", 28, 2);
//	return;

	int month, year;
	if (error = get_issue(&month, &year)) return error;
	printf("MAGIC DISK %02i/%02i\n", month, year);

	int yearmonth = year * 100 + month;
	if (yearmonth<8810) { /* 40 character screen based system */
		printf("MAGIC DISK 11/87 to 09/88 not yet supported!\n");
		return 4;
	} else if (yearmonth<8907) { /* 63 character screen based system */
		printf("MAGIC DISK 10/88 to 07/89 not yet supported!\n");
		return 4;
	} else if (yearmonth<9004) { /* modern system by Ivo Herzeg */
//		printf("MAGIC DISK 08/89 to 03/90 not yet supported!\n");
		return extract_md_C1a(month, year);
		return 4;
	} else if (yearmonth<9106) { /* modern system, no directory entries */
//		printf("MAGIC DISK 04/90 to 05/91 not yet supported!\n");
		return extract_md_C1b(month, year);
		return 4;
	} else if (yearmonth<9111) { /* icon system */
//		printf("MAGIC DISK 06/91 to 10/91 not yet supported!\n");
		return extract_md_C2b(month, year);
		return 4;
	} else if (yearmonth<9508) { /* icon system, new bootloader */
//		printf("MAGIC DISK 11/91 to 05/95 not yet supported!\n");
		return extract_md_C2b(month, year);
	} else {                     /* MAGIC DISK Classic */
		printf("MAGIC DISK 06/95 and later do not contain ant texts!\n");
		return 4;
	}

}

/*
        Track   Sectors/track   # Sectors   Storage in Bytes
        -----   -------------   ---------   ----------------
         1-17        21            357           7820
        18-24        19            133           7170
        25-30        18            108           6300
        31-40(*)     17             85           6020
                                   ---
                                   683 (for a 35 track image)

  Track #Sect #SectorsIn D64 Offset   Track #Sect #SectorsIn D64 Offset
  ----- ----- ---------- ----------   ----- ----- ---------- ----------
    1     21       0       $00000      21     19     414       $19E00
    2     21      21       $01500      22     19     433       $1B100
    3     21      42       $02A00      23     19     452       $1C400
    4     21      63       $03F00      24     19     471       $1D700
    5     21      84       $05400      25     18     490       $1EA00
    6     21     105       $06900      26     18     508       $1FC00
    7     21     126       $07E00      27     18     526       $20E00
    8     21     147       $09300      28     18     544       $22000
    9     21     168       $0A800      29     18     562       $23200
   10     21     189       $0BD00      30     18     580       $24400
   11     21     210       $0D200      31     17     598       $25600
   12     21     231       $0E700      32     17     615       $26700
   13     21     252       $0FC00      33     17     632       $27800
   14     21     273       $11100      34     17     649       $28900
   15     21     294       $12600      35     17     666       $29A00
   16     21     315       $13B00      36(*)  17     683       $2AB00
   17     21     336       $15000      37(*)  17     700       $2BC00
   18     19     357       $16500      38(*)  17     717       $2CD00
   19     19     376       $17800      39(*)  17     734       $2DE00
   20     19     395       $18B00      40(*)  17     751       $2EF00

  (*)Tracks 36-40 apply to 40-track images only
*/