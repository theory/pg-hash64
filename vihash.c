/*
 * vihash
 *
 * Portions Copyright (c) 2010, Peter Eisentraut
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 */

#include "postgres.h"
#include "fmgr.h"

PG_MODULE_MAGIC;


PG_FUNCTION_INFO_V1(hash64);

Datum hash64(PG_FUNCTION_ARGS);
Datum hash64_any(register const unsigned char *k, register int keylen);


Datum
hash64(PG_FUNCTION_ARGS)
{
	text	   *key = PG_GETARG_TEXT_PP(0);
	Datum		result;

	/*
	 * Note: this is currently identical in behavior to hashvarlena, but keep
	 * it as a separate function in case we someday want to do something
	 * different in non-C locales.	(See also hashbpchar, if so.)
	 */
	result = hash64_any((unsigned char *) VARDATA_ANY(key),
					  VARSIZE_ANY_EXHDR(key));

	/* Avoid leaking memory for toasted inputs */
	PG_FREE_IF_COPY(key, 0);

	return result;
}

/*
 * This hash function was written by Bob Jenkins
 * (bob_jenkins@burtleburtle.net), and superficially adapted
 * for PostgreSQL by Neil Conway. For more information on this
 * hash function, see http://burtleburtle.net/bob/hash/doobs.html,
 * or Bob's article in Dr. Dobb's Journal, Sept. 1997.
 *
 * In the current code, we have adopted Bob's 2006 update of his hash
 * function to fetch the data a word at a time when it is suitably aligned.
 * This makes for a useful speedup, at the cost of having to maintain
 * four code paths (aligned vs unaligned, and little-endian vs big-endian).
 * It also uses two separate mixing functions mix() and final(), instead
 * of a slower multi-purpose function.
 */

/* Get a bit mask of the bits set in non-uint32 aligned addresses */
#define UINT32_ALIGN_MASK (sizeof(uint32) - 1)

/* Rotate a uint32 value left by k bits - note multiple evaluation! */
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*----------
 * mix -- mix 3 32-bit values reversibly.
 *
 * This is reversible, so any information in (a,b,c) before mix() is
 * still in (a,b,c) after mix().
 *
 * If four pairs of (a,b,c) inputs are run through mix(), or through
 * mix() in reverse, there are at least 32 bits of the output that
 * are sometimes the same for one pair and different for another pair.
 * This was tested for:
 * * pairs that differed by one bit, by two bits, in any combination
 *	 of top bits of (a,b,c), or in any combination of bottom bits of
 *	 (a,b,c).
 * * "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
 *	 the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
 *	 is commonly produced by subtraction) look like a single 1-bit
 *	 difference.
 * * the base values were pseudorandom, all zero but one bit set, or
 *	 all zero plus a counter that starts at zero.
 *
 * This does not achieve avalanche.  There are input bits of (a,b,c)
 * that fail to affect some output bits of (a,b,c), especially of a.  The
 * most thoroughly mixed value is c, but it doesn't really even achieve
 * avalanche in c.
 *
 * This allows some parallelism.  Read-after-writes are good at doubling
 * the number of bits affected, so the goal of mixing pulls in the opposite
 * direction from the goal of parallelism.	I did what I could.  Rotates
 * seem to cost as much as shifts on every machine I could lay my hands on,
 * and rotates are much kinder to the top and bottom bits, so I used rotates.
 *----------
 */
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);	c += b; \
  b -= a;  b ^= rot(a, 6);	a += c; \
  c -= b;  c ^= rot(b, 8);	b += a; \
  a -= c;  a ^= rot(c,16);	c += b; \
  b -= a;  b ^= rot(a,19);	a += c; \
  c -= b;  c ^= rot(b, 4);	b += a; \
}

/*----------
 * final -- final mixing of 3 32-bit values (a,b,c) into c
 *
 * Pairs of (a,b,c) values differing in only a few bits will usually
 * produce values of c that look totally different.  This was tested for
 * * pairs that differed by one bit, by two bits, in any combination
 *	 of top bits of (a,b,c), or in any combination of bottom bits of
 *	 (a,b,c).
 * * "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
 *	 the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
 *	 is commonly produced by subtraction) look like a single 1-bit
 *	 difference.
 * * the base values were pseudorandom, all zero but one bit set, or
 *	 all zero plus a counter that starts at zero.
 *
 * The use of separate functions for mix() and final() allow for a
 * substantial performance increase since final() does not need to
 * do well in reverse, but is does need to affect all output bits.
 * mix(), on the other hand, does not need to affect all output
 * bits (affecting 32 bits is enough).	The original hash function had
 * a single mixing operation that had to satisfy both sets of requirements
 * and was slower as a result.
 *----------
 */
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c, 4); \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
 * hash_any() -- hash a variable-length key into a 32-bit value
 *		k		: the key (the unaligned variable-length array of bytes)
 *		len		: the length of the key, counting by bytes
 *
 * Returns a uint32 value.	Every bit of the key affects every bit of
 * the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 * About 6*len+35 instructions. The best hash table sizes are powers
 * of 2.  There is no need to do mod a prime (mod is sooo slow!).
 * If you need less than 32 bits, use a bitmask.
 */
Datum
hash64_any(register const unsigned char *k, register int keylen)
{
    register uint32 a,
                b,
                c,
                len;

    /* Set up the internal state */
    len = keylen;
    a = b = c = 0x9e3779b9 + len + 3923095;

    /* If the source pointer is word-aligned, we use word-wide fetches */
    if (((intptr_t) k & UINT32_ALIGN_MASK) == 0)
    {
        /* Code path for aligned source data */
        register const uint32 *ka = (const uint32 *) k;

        /* handle most of the key */
        while (len >= 12)
        {
            a += ka[0];
            b += ka[1];
            c += ka[2];
            mix(a, b, c);
            ka += 3;
            len -= 12;
        }

        /* handle the last 11 bytes */
        k = (const unsigned char *) ka;
#ifdef WORDS_BIGENDIAN
        switch (len)
        {
            case 11:
                c += ((uint32) k[10] << 8);
                /* fall through */
            case 10:
                c += ((uint32) k[9] << 16);
                /* fall through */
            case 9:
                c += ((uint32) k[8] << 24);
                /* the lowest byte of c is reserved for the length */
                /* fall through */
            case 8:
                b += ka[1];
                a += ka[0];
                break;
            case 7:
                b += ((uint32) k[6] << 8);
                /* fall through */
            case 6:
                b += ((uint32) k[5] << 16);
                /* fall through */
            case 5:
                b += ((uint32) k[4] << 24);
                /* fall through */
            case 4:
                a += ka[0];
                break;
            case 3:
                a += ((uint32) k[2] << 8);
                /* fall through */
            case 2:
                a += ((uint32) k[1] << 16);
                /* fall through */
            case 1:
                a += ((uint32) k[0] << 24);
                /* case 0: nothing left to add */
        }
#else                           /* !WORDS_BIGENDIAN */
        switch (len)
        {
            case 11:
                c += ((uint32) k[10] << 24);
                /* fall through */
            case 10:
                c += ((uint32) k[9] << 16);
                /* fall through */
            case 9:
                c += ((uint32) k[8] << 8);
                /* the lowest byte of c is reserved for the length */
                /* fall through */
            case 8:
                b += ka[1];
                a += ka[0];
                break;
            case 7:
                b += ((uint32) k[6] << 16);
                /* fall through */
            case 6:
                b += ((uint32) k[5] << 8);
                /* fall through */
            case 5:
                b += k[4];
                /* fall through */
            case 4:
                a += ka[0];
                break;
            case 3:
                a += ((uint32) k[2] << 16);
                /* fall through */
            case 2:
                a += ((uint32) k[1] << 8);
                /* fall through */
            case 1:
                a += k[0];
                /* case 0: nothing left to add */
        }
#endif   /* WORDS_BIGENDIAN */
    }
    else
    {
        /* Code path for non-aligned source data */

        /* handle most of the key */
        while (len >= 12)
        {
#ifdef WORDS_BIGENDIAN
            a += (k[3] + ((uint32) k[2] << 8) + ((uint32) k[1] << 16) + ((uint32) k[0] << 24));
            b += (k[7] + ((uint32) k[6] << 8) + ((uint32) k[5] << 16) + ((uint32) k[4] << 24));
            c += (k[11] + ((uint32) k[10] << 8) + ((uint32) k[9] << 16) + ((uint32) k[8] << 24));
#else                           /* !WORDS_BIGENDIAN */
            a += (k[0] + ((uint32) k[1] << 8) + ((uint32) k[2] << 16) + ((uint32) k[3] << 24));
            b += (k[4] + ((uint32) k[5] << 8) + ((uint32) k[6] << 16) + ((uint32) k[7] << 24));
            c += (k[8] + ((uint32) k[9] << 8) + ((uint32) k[10] << 16) + ((uint32) k[11] << 24));
#endif   /* WORDS_BIGENDIAN */
            mix(a, b, c);
            k += 12;
            len -= 12;
        }

        /* handle the last 11 bytes */
#ifdef WORDS_BIGENDIAN
        switch (len)            /* all the case statements fall through */
        {
            case 11:
                c += ((uint32) k[10] << 8);
            case 10:
                c += ((uint32) k[9] << 16);
            case 9:
                c += ((uint32) k[8] << 24);
                /* the lowest byte of c is reserved for the length */
            case 8:
                b += k[7];
            case 7:
                b += ((uint32) k[6] << 8);
            case 6:
                b += ((uint32) k[5] << 16);
            case 5:
                b += ((uint32) k[4] << 24);
            case 4:
                a += k[3];
            case 3:
                a += ((uint32) k[2] << 8);
            case 2:
                a += ((uint32) k[1] << 16);
            case 1:
                a += ((uint32) k[0] << 24);
                /* case 0: nothing left to add */
        }
#else                           /* !WORDS_BIGENDIAN */
        switch (len)            /* all the case statements fall through */
        {
            case 11:
                c += ((uint32) k[10] << 24);
            case 10:
                c += ((uint32) k[9] << 16);
            case 9:
                c += ((uint32) k[8] << 8);
                /* the lowest byte of c is reserved for the length */
            case 8:
                b += ((uint32) k[7] << 24);
            case 7:
                b += ((uint32) k[6] << 16);
            case 6:
                b += ((uint32) k[5] << 8);
            case 5:
                b += k[4];
            case 4:
                a += ((uint32) k[3] << 24);
            case 3:
                a += ((uint32) k[2] << 16);
            case 2:
                a += ((uint32) k[1] << 8);
            case 1:
                a += k[0];
                /* case 0: nothing left to add */
        }
#endif   /* WORDS_BIGENDIAN */
    }

    final(a, b, c);

    /* report the result */
    /* return UInt32GetDatum(c); */
    return Int64GetDatum((uint64) b |
                         (((uint64) c) << (sizeof(uint64) / 2) * BITS_PER_BYTE));
}
