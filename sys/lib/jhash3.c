/*
 * lookup3.c, by Bob Jenkins, May 2006, Public Domain.
 *
 * These are functions for producing 32-bit hashes for hash table lookup.
 * hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
 * are externally useful functions.  Routines to test the hash are included
 * if SELF_TEST is defined.  You can use this free for any purpose.  It's in
 * the public domain.  It has no warranty.
 *
 * Adapted for use in APEX by Patrick Oppenlander on 29/7/2011.
 * The original source can be found here
 *   http://burtleburtle.net/bob/c/lookup3.c
 */

#include <jhash3.h>

#include <endian.h>

#define jhash_rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define jhash_mix(a,b,c) { \
	a -= c;  a ^= jhash_rot(c, 4);  c += b; \
	b -= a;  b ^= jhash_rot(a, 6);  a += c; \
	c -= b;  c ^= jhash_rot(b, 8);  b += a; \
	a -= c;  a ^= jhash_rot(c,16);  c += b; \
	b -= a;  b ^= jhash_rot(a,19);  a += c; \
	c -= b;  c ^= jhash_rot(b, 4);  b += a; \
}

#define jhash_final(a,b,c) { \
	c ^= b; c -= jhash_rot(b,14); \
	a ^= c; a -= jhash_rot(c,11); \
	b ^= a; b -= jhash_rot(a,25); \
	c ^= b; c -= jhash_rot(b,16); \
	a ^= c; a -= jhash_rot(c,4);  \
	b ^= a; b -= jhash_rot(a,14); \
	c ^= b; c -= jhash_rot(b,24); \
}

/*
 * jhash32: Hash an array of uint32_t's.
 *
 * To be useful, it requires:
 * - that the key be an array of uint32_t's, and
 * - that the length be the number of uint32_t's in the key.
 */
uint32_t jhash32(const uint32_t *k, size_t length, uint32_t initval)
{
	uint32_t a, b, c;

	/* set up the internal state */
	a = b = c = 0xdeadbeef + (((uint32_t)length) << 2) + initval;

	/* handle most of the key */
	while (length > 3) {
		a += k[0];
		b += k[1];
		c += k[2];
		jhash_mix(a,b,c);
		length -= 3;
		k += 3;
	}

	/* handle the last 3 uint32_t's */
	switch (length) {
	case 3: c += k[2];
	case 2: b += k[1];
	case 1: a += k[0];
		jhash_final(a,b,c);
	}

	return c;
}

/*
 * jhash: Hash an array of data.
 *
 * k: the key (variable length array of bytes)
 * length: length of the key, in bytes
 * initval: any 4 byte value
 */
uint32_t jhash(const void *key, size_t length, uint32_t initval)
{
	uint32_t a, b, c;

	/* set up the internal state */
	a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

	/* read 32-bit chunks */
	const uint32_t *k = (const uint32_t *)key;

	/* all but last block: reads and affect 32 bits of (a,b,c) */
	while (length > 12) {
		a += k[0];
		b += k[1];
		c += k[2];
		jhash_mix(a,b,c);
		length -= 12;
		k += 3;
	}

	/* handle the last (probably partial) block
	 *
	 * this code actually reads beyond the end of the string, but
	 * then masks off the part it's not allowed to read.  Because the
	 * string is aligned, the masked-off tail is in the same word as the
	 * rest of the string.  Every machine with memory protection I've seen
	 * does it on word boundaries, so is OK with this.  But VALGRIND will
	 * still catch it and complain.  The masking trick does make the hash
	 * noticably faster for short strings (like English words).
	 *
	 * REVISIT: yeah, but what if the array isn't aligned?
	 */
	if (BYTE_ORDER == LITTLE_ENDIAN) {
		switch (length) {
		case 12:    c += k[2];		    b += k[1];		    a += k[0];		    break;
		case 11:    c += k[2] & 0xffffff;   b += k[1];		    a += k[0];		    break;
		case 10:    c += k[2] & 0xffff;	    b += k[1];		    a += k[0];		    break;
		case 9:	    c += k[2] & 0xff;	    b += k[1];		    a += k[0];		    break;
		case 8:				    b += k[1];		    a += k[0];		    break;
		case 7:				    b += k[1] & 0xffffff;   a += k[0];		    break;
		case 6:				    b += k[1] & 0xffff;	    a += k[0];		    break;
		case 5:				    b += k[1] & 0xff;	    a += k[0];		    break;
		case 4:							    a += k[0];		    break;
		case 3:							    a += k[0] & 0xffffff;   break;
		case 2:							    a += k[0] & 0xffff;	    break;
		case 1:							    a += k[0] & 0xff;	    break;
		}
	} else {
		switch (length) {
		case 12:    c += k[2];		    b += k[1];		    a += k[0];		    break;
		case 11:    c += k[2] & 0xffffff00; b += k[1];		    a += k[0];		    break;
		case 10:    c += k[2] & 0xffff0000; b += k[1];		    a += k[0];		    break;
		case 9:	    c += k[2] & 0xff000000; b += k[1];		    a += k[0];		    break;
		case 8:				    b += k[1];		    a += k[0];		    break;
		case 7:				    b += k[1] & 0xffffff00; a += k[0];		    break;
		case 6:				    b += k[1] & 0xffff0000; a += k[0];		    break;
		case 5:				    b += k[1] & 0xff000000; a += k[0];		    break;
		case 4:							    a += k[0];		    break;
		case 3:							    a += k[0] & 0xffffff00; break;
		case 2:							    a += k[0] & 0xffff0000; break;
		case 1:							    a += k[0] & 0xff000000; break;
		}
	}

	jhash_final(a,b,c);
	return c;
}

/*
 * jhash_string: Hash a string.
 *
 * k: the key (a string)
 * initval: any 4 byte value
 */
#define jhash_haszero(v) (((v) - 0x01010101UL) & ~(v) & 0x80808080UL)
uint32_t jhash_string(const char *key, uint32_t initval)
{
	uint32_t a, b, c;

	/* set up the internal state */
	a = b = c = 0xdeadbeef + initval;

	/* read 32-bit chunks */
	const uint32_t *k = (const uint32_t *)key;

	/* all but last block: reads and affect 32 bits of (a,b,c) */
	int z0, z1, z2;
	while (1) {
		if ((z0 = jhash_haszero(k[0]))) break;
		if ((z1 = jhash_haszero(k[1]))) break;
		if ((z2 = jhash_haszero(k[2]))) break;
		if (BYTE_ORDER == LITTLE_ENDIAN) {
			if (!(k[3] & 0xff)) break;
		} else {
			if (!(k[3] & 0xff000000)) break;
		}
		a += k[0];
		b += k[1];
		c += k[2];
		jhash_mix(a,b,c);
		k += 3;
	}

	/* determine remaining length */
	size_t length;
	if (BYTE_ORDER == LITTLE_ENDIAN) {
		if (z0) {
			if (!(k[0] & 0x000000ff)) { length = 0; goto out; }
			if (!(k[0] & 0x0000ff00)) { length = 1; goto out; }
			if (!(k[0] & 0x00ff0000)) { length = 2;	goto out; }
			length = 3; goto out;
		}
		if (z1) {
			if (!(k[1] & 0x000000ff)) { length = 4; goto out; }
			if (!(k[1] & 0x0000ff00)) { length = 5; goto out; }
			if (!(k[1] & 0x00ff0000)) { length = 6;	goto out; }
			length = 7; goto out;
		}
		if (z2) {
			if (!(k[2] & 0x000000ff)) { length = 8; goto out; }
			if (!(k[2] & 0x0000ff00)) { length = 9; goto out; }
			if (!(k[2] & 0x00ff0000)) { length = 10;goto out; }
			length = 11; goto out;
		}
		length = 12;
	} else {
		if (z0) {
			if (!(k[0] & 0xff000000)) { length = 0; goto out; }
			if (!(k[0] & 0x00ff0000)) { length = 1; goto out; }
			if (!(k[0] & 0x0000ff00)) { length = 2;	goto out; }
			length = 3; goto out;
		}
		if (z1) {
			if (!(k[1] & 0xff000000)) { length = 4; goto out; }
			if (!(k[1] & 0x00ff0000)) { length = 5; goto out; }
			if (!(k[1] & 0x0000ff00)) { length = 6;	goto out; }
			length = 7; goto out;
		}
		if (z2) {
			if (!(k[2] & 0xff000000)) { length = 8; goto out; }
			if (!(k[2] & 0x00ff0000)) { length = 9; goto out; }
			if (!(k[2] & 0x0000ff00)) { length = 10;goto out; }
			length = 11; goto out;
		}
		length = 12;
	}

out:

	/* handle the last (probably partial) block
	 *
	 * this code actually reads beyond the end of the string, but
	 * then masks off the part it's not allowed to read.  Because the
	 * string is aligned, the masked-off tail is in the same word as the
	 * rest of the string.  Every machine with memory protection I've seen
	 * does it on word boundaries, so is OK with this.  But VALGRIND will
	 * still catch it and complain.  The masking trick does make the hash
	 * noticably faster for short strings (like English words).
	 *
	 * REVISIT: yeah, but who says the fucking array is aligned.
	 */
	if (BYTE_ORDER == LITTLE_ENDIAN) {
		switch (length) {
		case 12:    c += k[2];		    b += k[1];		    a += k[0];		    break;
		case 11:    c += k[2] & 0xffffff;   b += k[1];		    a += k[0];		    break;
		case 10:    c += k[2] & 0xffff;	    b += k[1];		    a += k[0];		    break;
		case 9:	    c += k[2] & 0xff;	    b += k[1];		    a += k[0];		    break;
		case 8:				    b += k[1];		    a += k[0];		    break;
		case 7:				    b += k[1] & 0xffffff;   a += k[0];		    break;
		case 6:				    b += k[1] & 0xffff;	    a += k[0];		    break;
		case 5:				    b += k[1] & 0xff;	    a += k[0];		    break;
		case 4:							    a += k[0];		    break;
		case 3:							    a += k[0] & 0xffffff;   break;
		case 2:							    a += k[0] & 0xffff;	    break;
		case 1:							    a += k[0] & 0xff;	    break;
		}
	} else {
		switch (length) {
		case 12:    c += k[2];		    b += k[1];		    a += k[0];		    break;
		case 11:    c += k[2] & 0xffffff00; b += k[1];		    a += k[0];		    break;
		case 10:    c += k[2] & 0xffff0000; b += k[1];		    a += k[0];		    break;
		case 9:	    c += k[2] & 0xff000000; b += k[1];		    a += k[0];		    break;
		case 8:				    b += k[1];		    a += k[0];		    break;
		case 7:				    b += k[1] & 0xffffff00; a += k[0];		    break;
		case 6:				    b += k[1] & 0xffff0000; a += k[0];		    break;
		case 5:				    b += k[1] & 0xff000000; a += k[0];		    break;
		case 4:							    a += k[0];		    break;
		case 3:							    a += k[0] & 0xffffff00; break;
		case 2:							    a += k[0] & 0xffff0000; break;
		case 1:							    a += k[0] & 0xff000000; break;
		}
	}

	jhash_final(a,b,c);
	return c;
}

/*
 * jhash_nwords: Optimised version for 3, 2 and 1 word(s).
 */
uint32_t jhash_3words(uint32_t a, uint32_t b, uint32_t c)
{
	jhash_mix(a, b, c);
	jhash_final(a, b, c);
	return c;
}

uint32_t jhash_2words(uint32_t a, uint32_t b)
{
	return jhash_3words(a, b, 0);

}

uint32_t jhash_1word(uint32_t a)
{
	return jhash_2words(a, 0);
}
