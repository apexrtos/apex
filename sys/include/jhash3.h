#ifndef jhash3_h
#define jhash3_h

/*
 * Adapted for use in APEX by Patrick Oppenlander on 29/7/2011.
 * The original source can be found here
 *   http://burtleburtle.net/bob/c/lookup3.c
 *
 * lookup3.c, by Bob Jenkins, May 2006, Public Domain.
 *
 * These are functions for producing 32-bit hashes for hash table lookup.
 * hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
 * are externally useful functions.  Routines to test the hash are included
 * if SELF_TEST is defined.  You can use this free for any purpose.  It's in
 * the public domain.  It has no warranty.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * jhash32: Hash an array of uint32_t's.
 *
 * To be useful, it requires:
 * - that the key be an array of uint32_t's, and
 * - that the length be the number of uint32_t's in the key.
 */
uint32_t jhash32(const uint32_t *k, size_t length, uint32_t initval);

/*
 * jhash: Hash an array of data.
 *
 * k: the key (variable length array of bytes)
 * length: length of the key, in bytes
 * initval: any 4 byte value
 */
uint32_t jhash(const void *key, size_t length, uint32_t initval);

/*
 * jhash_string: Hash a string.
 *
 * k: the key (a string)
 * initval: any 4 byte value
 */
uint32_t jhash_string(const char *key, uint32_t initval);

/*
 * jhash_nwords: Optimised version for 3, 2 and 1 word(s).
 */
uint32_t jhash_3words(uint32_t a, uint32_t b, uint32_t c);
uint32_t jhash_2words(uint32_t a, uint32_t b);
uint32_t jhash_1word(uint32_t a);

#endif /* !jhash3_h */
