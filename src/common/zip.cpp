#include "common.h"

/* Inflate (gzip/zlib-style) used to decompress game data. */

extern const unsigned short mask_bits[17];
extern const unsigned int c_border[19];
extern const unsigned short c_plens[31];
extern const unsigned short c_plext[31];
extern const unsigned short c_pdist[30];
extern const unsigned short c_pdext[30];
extern const unsigned long crc_32_tab[256];

/* gzip inflate.c Huffman table entry */
struct huft {
    unsigned char e; /* number of extra bits or operation */
    unsigned char b; /* number of bits in this code or subcode */
    union {
        unsigned short n;  /* literal, length base, or distance base */
        struct huft *t;    /* pointer to next level of table */
    } v;
};

extern "C" {
int printf(const char *, ...);
void *malloc(unsigned int);
void free(void *);
void *memcpy(void *, const void *, unsigned int);
}
int fileReads(char *name, void *buf, unsigned int block);

unsigned long zipCrc32(unsigned long crc, const unsigned char *buf, long len);
int zipFlush(unsigned long w);
int zipCheckHeader(void);
int zipInflateCodes(struct huft *tl, struct huft *td, int bl, int bd);
int zipFreeHuffmanTable(struct huft *t);
int zipBuildHuffmanTable(const unsigned int *b, unsigned int n, unsigned int s, const unsigned short *d,
                         const unsigned short *e, struct huft **t, int *m);
int zipInflateBlockStored(void);
int zipInflateBlockFixed(void);
int zipInflateBlockDynamic(void);
int zipInflateBlock(int *e);
int zipInflateAll(char *name, void *dest);
unsigned char zipGetChar(void);

/* gzip inflate.c bit buffer macros */
#define NEXTBYTE() ((unsigned char)zipGetChar())
#define NEEDBITS(n) { while (k < (n)) { b |= ((unsigned long)NEXTBYTE()) << k; k += 8; } }
#define DUMPBITS(n) { b >>= (n); k -= (n); }

#define ZIP_BUF ((unsigned char *)0x01F7F840) /* 512 KB read window at the top of RAM */
#define ZIP_BUF_END (ZIP_BUF + 0x80000)
#define WSIZE 0x80000 /* output window flushed by zipFlush() */
#define BMAX 16       /* maximum bit length of any code */
#define N_MAX 288     /* maximum number of codes in any set */

int whichHalfMeg = 0;
unsigned int G_windowPos = 0;
unsigned int G_bitCount = 0;
unsigned int G_bitBucket = 0;
unsigned long G_crc32val = 0;
struct huft *G_fixedTlen = 0;
char *G_FileName = 0;
void *G_FileAddr = 0;
int tmpLong;
unsigned long G_outSize;
struct huft *G_fixedTdist;
int G_fixedBlen;
int G_fixedBdist;
int howManyBlocks;
unsigned char *zipFileBuf;
unsigned char *outFileBuf;
unsigned char *outFileWindow = outFileBuf;

/* zlib crc32 */
#define DO1(buf) crc = tab[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf) DO1(buf); DO1(buf);
#define DO4(buf) DO2(buf); DO2(buf);
#define DO8(buf) DO4(buf); DO4(buf);

unsigned long zipCrc32(unsigned long crc, const unsigned char *buf, long len)
{
    if (buf == NULL) return 0L;
    const unsigned long *tab = crc_32_tab;
    crc = crc ^ 0xffffffffL;
    while (len >= 8) {
        DO8(buf);
        len -= 8;
    }
    if (len) do {
        DO1(buf);
    } while (--len);
    return crc ^ 0xffffffffL;
}

int zipFlush(unsigned long w)
{
    G_crc32val = zipCrc32(G_crc32val, outFileBuf, w);
    outFileBuf += (int)w;
    G_outSize -= w;
    return 0;
}

#ifdef NON_MATCHING
/* scheduling/register allocation of the size bytes differs */
/* Parses the zip local file header at the start of the read window (signature "IE" instead of "PK").
 * Leaves zipFileBuf at the compressed data and returns the uncompressed size. */
int zipCheckHeader(void)
{
    int sig;
    int ret;

    zipFileBuf = ZIP_BUF;
    sig = *(int *)zipFileBuf;
    if (sig == 0x04034549) {
        unsigned char *size = ZIP_BUF + 22;
        unsigned short *nameLen = (unsigned short *)(ZIP_BUF + 26);
        ret = size[0] + (size[1] << 8) + (size[2] << 16) + (size[3] << 24);
        zipFileBuf = (unsigned char *)nameLen + (*nameLen + 4);
    } else {
        printf("File signature = 0x%X, should be 0x04034549 (\"IE..\")\n", sig);
        ret = 0;
    }
    return ret;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/zip", zipCheckHeader__Fv);
#endif

/* inflate (decompress) the codes in a deflated (compressed) block until an end-of-block code */
int zipInflateCodes(struct huft *tl, struct huft *td, int bl, int bd)
{
    unsigned e;      /* table entry flag/number of extra bits */
    unsigned n, d;   /* length and index for copy */
    unsigned w;      /* current window position */
    struct huft *t;  /* pointer to table entry */
    unsigned ml, md; /* masks for bl and bd bits */
    unsigned long b; /* bit buffer */
    unsigned k;      /* number of bits in bit buffer */
    int r = 0;

    b = G_bitBucket;
    k = G_bitCount;
    w = G_windowPos;

    ml = mask_bits[bl];
    md = mask_bits[bd];
    for (;;) {
        NEEDBITS((unsigned)bl)
        if ((e = (t = tl + ((unsigned)b & ml))->e) > 16)
            do {
                if (e == 99)
                    return 1;
                DUMPBITS(t->b)
                e -= 16;
                NEEDBITS(e)
            } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
        DUMPBITS(t->b)
        if (e == 16) { /* then it's a literal */
            outFileBuf[w++] = (unsigned char)t->v.n;
            if (w == WSIZE) {
                r = zipFlush(w);
                if (r != 0)
                    return r;
                w = 0;
            }
        } else { /* it's an EOB or a length */
            if (e == 15)
                break;

            /* get length of block to copy */
            NEEDBITS(e)
            n = t->v.n + ((unsigned)b & mask_bits[e]);
            DUMPBITS(e)

            /* decode distance of block to copy */
            NEEDBITS((unsigned)bd)
            if ((e = (t = td + ((unsigned)b & md))->e) > 16)
                do {
                    if (e == 99)
                        return 1;
                    DUMPBITS(t->b)
                    e -= 16;
                    NEEDBITS(e)
                } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
            DUMPBITS(t->b)
            NEEDBITS(e)
            d = w - t->v.n - ((unsigned)b & mask_bits[e]);
            DUMPBITS(e)

            /* do the copy; the window before the current one sits right below outFileBuf */
            do {
                if ((d &= WSIZE - 1) >= w)
                    outFileWindow = outFileBuf - WSIZE;
                else
                    outFileWindow = outFileBuf;
                n -= (e = (e = WSIZE - (d > w ? d : w)) > n ? n : e);
                if (w - d >= e) {
                    memcpy(outFileBuf + w, outFileWindow + d, e);
                    w += e;
                    d += e;
                } else {
                    do {
                        outFileBuf[w++] = outFileWindow[d++];
                    } while (--e);
                }
                if (w == WSIZE) {
                    r = zipFlush(w);
                    if (r != 0)
                        return r;
                    w = 0;
                }
            } while (n);
        }
    }

    G_windowPos = w;
    G_bitBucket = b;
    G_bitCount = k;
    return r;
}

int zipFreeHuffmanTable(struct huft *t)
{
    struct huft *p, *q;

    p = t;
    while (p != NULL) {
        q = (--p)->v.t;
        free(p);
        p = q;
    }
    return 0;
}

/* Given a list of code lengths and a maximum table size, make a set of tables to decode that set of codes
 * (Info-ZIP unzip inflate.c huft_build). Returns 0 on success, 1 if the given code set is incomplete,
 * 2 if the input is invalid, 3 if not enough memory. */
int zipBuildHuffmanTable(const unsigned int *b, unsigned int n, unsigned int s, const unsigned short *d,
                         const unsigned short *e, struct huft **t, int *m)
{
    unsigned a;           /* counter for codes of length k */
    unsigned c[BMAX + 1]; /* bit length count table */
    unsigned el;          /* length of EOB code (value 256) */
    unsigned f;           /* i repeats in table every f entries */
    int g;                /* maximum code length */
    int h;                /* table level */
    unsigned i;           /* counter, current code */
    unsigned j;           /* counter */
    int k;                /* number of bits in current code */
    int lx[BMAX + 1];     /* memory for l[-1..BMAX-1] */
    int *l = lx + 1;      /* stack of bits per table */
    const unsigned *p;    /* pointer into c[], b[], or v[] */
    struct huft *q;       /* points to current table */
    struct huft r;        /* table entry for structure assignment */
    struct huft *u[BMAX]; /* table stack */
    unsigned v[N_MAX];    /* values in order of bit length */
    int w;                /* bits before this table == (l * h) */
    unsigned x[BMAX + 1]; /* bit offsets, then code stack */
    unsigned *xp;         /* pointer into x */
    int y;                /* number of dummy codes added */
    unsigned z;           /* number of entries in current table */

    /* Generate counts for each bit length */
    el = n > 256 ? b[256] : BMAX; /* set length of EOB code, if any */
    for (i = 0; i < BMAX + 1; i++)
        c[i] = 0;
    p = b;
    i = n;
    while (i--) {
        c[*p]++; /* assume all entries <= BMAX */
        p++;
    }
    if (c[0] == n) { /* null input--all zero length codes */
        *t = NULL;
        *m = 0;
        printf("The damn codes are empty!!.\n");
        return 0;
    }

    /* Find minimum and maximum length, bound *m by those */
    for (j = 1; j <= BMAX; j++)
        if (c[j])
            break;
    k = j; /* minimum code length */
    if ((unsigned)*m < j)
        *m = j;
    for (i = BMAX; i; i--)
        if (c[i])
            break;
    g = i; /* maximum code length */
    if ((unsigned)*m > i)
        *m = i;

    /* Adjust last length count to fill out codes, if needed */
    for (y = 1 << j; j < i; y <<= 1, j++)
        if ((y -= c[j]) < 0) {
            printf("Too many codes, not enough bits., numDummies = %i, a_Counts[j] = %i\n", y, c[j]);
            return 2; /* bad input: more codes than bits */
        }
    if ((y -= c[i]) < 0) {
        printf("Too many codes, not enough bits.ZZ, numDummies = %i, a_Counts[j] = %i\n", y, c[j]);
        return 2;
    }
    c[i] += y;

    /* Generate starting offsets into the value table for each length */
    x[1] = j = 0;
    p = c + 1;
    xp = x + 2;
    while (--i) { /* note that i == g from above */
        *xp++ = (j += *p++);
    }

    /* Make a table of values in order of bit lengths */
    for (i = 0; i < N_MAX; i++)
        v[i] = 0;
    p = b;
    for (i = 0; i < n; i++) {
        if ((j = *p++) != 0) {
            v[x[j]] = i;
            x[j]++;
        }
    }
    n = x[g]; /* set n to length of v */

    /* Generate the Huffman codes and for each, make the table entries */
    x[0] = i = 0;  /* first Huffman code is zero */
    p = v;         /* grab values in bit order */
    h = -1;        /* no tables yet--level -1 */
    w = l[-1] = 0; /* no bits decoded yet */
    u[0] = NULL;   /* just to keep compilers happy */
    q = NULL;      /* ditto */
    z = 0;         /* ditto */

    /* go through the bit lengths (k already is bits in shortest code) */
    for (; k <= g; k++) {
        a = c[k];
        while (a--) {
            /* here i is the Huffman code of length k bits for value *p */
            /* make tables up to required level */
            while (k > w + l[h]) {
                w += l[h++]; /* add bits already decoded */

                /* compute minimum size table less than or equal to *m bits */
                z = g - w;
                if (z > (unsigned)*m)
                    z = *m; /* upper limit */
                if ((f = 1 << (j = k - w)) > a + 1) {   /* try a k-w bit table */
                    /* too few codes for k-w bit table */
                    f -= a + 1; /* deduct codes from patterns left */
                    xp = c + k;
                    while (++j < z) { /* try smaller tables up to z bits */
                        if ((f <<= 1) <= *++xp)
                            break; /* enough codes to use up j bits */
                        f -= *xp;  /* else deduct codes from patterns */
                    }
                }
                if ((unsigned)w + j > el && (unsigned)w < el)
                    j = el - w; /* make EOB code end at table */
                z = 1 << j;     /* table entries for j-bit table */
                l[h] = j;       /* set table size in stack */

                /* allocate and link in new table */
                if ((q = (struct huft *)malloc((z + 1) * sizeof(struct huft))) == NULL) {
                    printf("Stupid problems with mallocing new tables\n");
                    if (h)
                        zipFreeHuffmanTable(u[0]);
                    return 3; /* not enough memory */
                }
                *t = q + 1; /* link to list for zipFreeHuffmanTable() */
                *(t = &(q->v.t)) = NULL;
                u[h] = ++q; /* table starts after link */

                /* connect to last table, if there is one */
                if (h) {
                    x[h] = i;                      /* save pattern for backing up */
                    r.b = (unsigned char)l[h - 1]; /* bits to dump before this table */
                    r.e = (unsigned char)(16 + j); /* bits in this table */
                    r.v.t = q;                     /* pointer to this table */
                    j = (i & ((1 << w) - 1)) >> (w - l[h - 1]);
                    u[h - 1][j] = r; /* connect to last table */
                }
            }

            /* set up table entry in r */
            r.b = (unsigned char)(k - w);
            if (p >= v + n) {
                r.e = 99; /* out of values--invalid code */
            } else if (*p < s) {
                r.e = (unsigned char)(*p < 256 ? 16 : 15); /* 256 is end-of-block code */
                r.v.n = (unsigned short)*p++;              /* simple code is just the value */
            } else {
                r.e = (unsigned char)e[*p - s]; /* non-simple--look up in lists */
                r.v.n = d[*p++ - s];
            }

            /* fill code-like entries with r */
            f = 1 << (k - w);
            for (j = i >> w; j < z; j += f)
                q[j] = r;

            /* backwards increment the k-bit code i */
            for (j = 1 << (k - 1); i & j; j >>= 1)
                i ^= j;
            i ^= j;

            /* backup over finished tables */
            while ((i & ((1 << w) - 1)) != x[h])
                w -= l[--h]; /* no need to update q */
        }
    }

    /* return actual size of base table */
    *m = l[0];

    /* Return true (1) if we were given an incomplete table */
    return y != 0 && g != 1;
}

/* "decompress" an inflated type 0 (stored) block */
int zipInflateBlockStored(void)
{
    unsigned n;
    unsigned w;
    unsigned long b;
    unsigned k;
    int r = 0;

    b = G_bitBucket;
    k = G_bitCount;
    w = G_windowPos;

    n = k & mask_bits[3];
    DUMPBITS(n)

    NEEDBITS(16)
    n = ((unsigned)b & 0xffff);
    DUMPBITS(16)
    NEEDBITS(16)
    if (n != (unsigned)((~b) & 0xffff)) {
        printf("Error in compressed data, stored block.\n");
        return 1;
    }
    DUMPBITS(16)

    while (n--) {
        NEEDBITS(8)
        outFileBuf[w++] = (unsigned char)b;
        if (w == WSIZE) {
            r = zipFlush(w);
            if (r != 0)
                return r;
            w = 0;
        }
        DUMPBITS(8)
    }

    G_windowPos = w;
    G_bitBucket = b;
    G_bitCount = k;
    return r;
}

/* decompress an inflated type 1 (fixed Huffman codes) block */
int zipInflateBlockFixed(void)
{
    /* if first time, set up tables for fixed blocks */
    if (G_fixedTlen == NULL) {
        int i;
        unsigned l[288]; /* length list for zipBuildHuffmanTable */

        for (i = 0; i < 144; i++)
            l[i] = 8;
        for (; i < 256; i++)
            l[i] = 9;
        for (; i < 280; i++)
            l[i] = 7;
        for (; i < 288; i++) /* make a complete, but wrong code set */
            l[i] = 8;
        G_fixedBlen = 7;
        if ((i = zipBuildHuffmanTable(l, 288, 257, c_plens, c_plext, &G_fixedTlen, &G_fixedBlen)) != 0) {
            G_fixedTlen = NULL;
            return i;
        }

        for (i = 0; i < 30; i++) /* make an incomplete code set */
            l[i] = 5;
        G_fixedBdist = 5;
        if ((i = zipBuildHuffmanTable(l, 30, 0, c_pdist, c_pdext, &G_fixedTdist, &G_fixedBdist)) > 1) {
            zipFreeHuffmanTable(G_fixedTlen);
            G_fixedTlen = NULL;
            G_fixedTdist = NULL;
            return i;
        }
    }

    /* decompress until an end-of-block code */
    return zipInflateCodes(G_fixedTlen, G_fixedTdist, G_fixedBlen, G_fixedBdist);
}

/* decompress an inflated type 2 (dynamic Huffman codes) block (gzip 1.3 inflate_dynamic) */
int zipInflateBlockDynamic(void)
{
    int i; /* temporary variables */
    unsigned j;
    unsigned l;        /* last length */
    unsigned m;        /* mask for bit lengths table */
    unsigned n;        /* number of lengths to get */
    struct huft *tl;   /* literal/length code table */
    struct huft *td;   /* distance code table */
    int bl;            /* lookup bits for tl */
    int bd;            /* lookup bits for td */
    unsigned nb;       /* number of bit length codes */
    unsigned nl;       /* number of literal/length codes */
    unsigned nd;       /* number of distance codes */
    unsigned ll[286 + 30]; /* literal/length and distance code lengths */
    unsigned long b;   /* bit buffer */
    unsigned k;        /* number of bits in bit buffer */

    /* make local bit buffer */
    b = G_bitBucket;
    k = G_bitCount;

    /* read in table lengths */
    NEEDBITS(5)
    nl = 257 + (b & mask_bits[5]); /* number of literal/length codes */
    DUMPBITS(5)
    NEEDBITS(5)
    nd = 1 + (b & mask_bits[5]); /* number of distance codes */
    DUMPBITS(5)
    NEEDBITS(4)
    nb = 4 + (b & mask_bits[4]); /* number of bit length codes */
    DUMPBITS(4)
    if (nl > 286 || nd > 30)
        return 1; /* bad lengths */

    /* read in bit-length-code lengths */
    for (j = 0; j < nb; j++) {
        NEEDBITS(3)
        ll[c_border[j]] = b & 7;
        DUMPBITS(3)
    }
    for (; j < 19; j++)
        ll[c_border[j]] = 0;

    /* build decoding table for trees--single level, 7 bit lookup */
    bl = 7;
    i = zipBuildHuffmanTable(ll, 19, 19, NULL, NULL, &tl, &bl);
    if (bl == 0) /* no bit lengths */
        i = 1;
    if (i) {
        if (i == 1)
            zipFreeHuffmanTable(tl);
        printf("!! Problems in building huffman tables. Error %i\n", i);
    }

    /* read in literal and distance code lengths */
    n = nl + nd;
    m = mask_bits[bl];
    i = l = 0;
    while ((unsigned)i < n) {
        NEEDBITS((unsigned)bl)
        j = (td = tl + ((unsigned)b & m))->b;
        DUMPBITS(j)
        j = td->v.n;
        if (j < 16) { /* length of code in bits (0..15) */
            ll[i++] = l = j; /* save last length in l */
        } else if (j == 16) { /* repeat last length 3 to 6 times */
            NEEDBITS(2)
            j = 3 + (b & mask_bits[2]);
            DUMPBITS(2)
            if ((unsigned)i + j > n)
                return 1;
            while (j--)
                ll[i++] = l;
        } else if (j == 17) { /* 3 to 10 zero length codes */
            NEEDBITS(3)
            j = 3 + (b & mask_bits[3]);
            DUMPBITS(3)
            if ((unsigned)i + j > n)
                return 1;
            while (j--)
                ll[i++] = 0;
            l = 0;
        } else { /* j == 18: 11 to 138 zero length codes */
            NEEDBITS(7)
            j = 11 + (b & mask_bits[7]);
            DUMPBITS(7)
            if ((unsigned)i + j > n)
                return 1;
            while (j--)
                ll[i++] = 0;
            l = 0;
        }
    }

    /* free decoding table for trees */
    zipFreeHuffmanTable(tl);

    /* restore the global bit buffer */
    G_bitBucket = b;
    G_bitCount = k;

    /* build the decoding tables for literal/length and distance codes */
    bl = 9;
    i = zipBuildHuffmanTable(ll, nl, 257, c_plens, c_plext, &tl, &bl);
    if (bl == 0) /* no literals or lengths */
        i = 1;
    if (i) {
        if (i == 1)
            zipFreeHuffmanTable(tl);
        return i; /* incomplete code set */
    }
    bd = 6;
    i = zipBuildHuffmanTable(ll + nl, nd, 0, c_pdist, c_pdext, &td, &bd);
    if (bd == 0 && nl > 257) /* lengths but no distances */
        i = 1;
    if (i) {
        if (i == 1)
            zipFreeHuffmanTable(td);
        zipFreeHuffmanTable(tl);
        return i; /* incomplete code set */
    }

    /* decompress until an end-of-block code */
    i = zipInflateCodes(tl, td, bl, bd);

    /* free the decoding tables, return */
    zipFreeHuffmanTable(tl);
    zipFreeHuffmanTable(td);
    return i;
}

/* decompress one block; *e is set on the last block */
int zipInflateBlock(int *e)
{
    unsigned t;
    unsigned long b;
    unsigned k;

    b = G_bitBucket;
    k = G_bitCount;

    NEEDBITS(1)
    *e = (int)b & mask_bits[1];
    DUMPBITS(1)

    NEEDBITS(2)
    t = (unsigned)b & mask_bits[2];
    DUMPBITS(2)

    G_bitBucket = b;
    G_bitCount = k;

    if (t == 0)
        return zipInflateBlockStored();
    if (t == 1)
        return zipInflateBlockFixed();
    if (t == 2)
        return zipInflateBlockDynamic();
    return 2;
}

/* decompress the zip file `name` to `dest`; returns the uncompressed size, 0 on failure */
int zipInflateAll(char *name, void *dest)
{
    int e = 0;
    int r;
    int size;

    G_FileName = name;
    G_FileAddr = dest;
    G_bitBucket = 0;
    G_bitCount = 0;
    G_windowPos = 0;
    whichHalfMeg = 0;
    outFileBuf = (unsigned char *)dest;
    outFileWindow = (unsigned char *)dest;
    printf("ZIP:- I received a request to unzip %s to %p.\n", name, dest);

    if (!fileReads(name, ZIP_BUF, whichHalfMeg++)) {
        printf("Couldn't read the file, \"%s\" into 0x%p\n", name, ZIP_BUF);
        return 0;
    }
    size = zipCheckHeader();
    if (size == 0) {
        printf("Header was screwed up, not a valid zip file.\n");
        return 0;
    }
    howManyBlocks = 0;
    do {
        howManyBlocks++;
        r = zipInflateBlock(&e);
        if (r != 0)
            return r;
    } while (!e);
    zipFlush(G_windowPos);
    return size;
}

#ifdef NON_MATCHING
/* register allocation differs: the reload constant lands in $v0 instead of $v1 */
unsigned char zipGetChar(void)
{
    if (zipFileBuf >= ZIP_BUF_END) {
        fileReads(G_FileName, ZIP_BUF, whichHalfMeg++);
        zipFileBuf = ZIP_BUF;
    }
    return *zipFileBuf++;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/zip", zipGetChar__Fv);
#endif


const unsigned short mask_bits[17] = {
    0x0000,
    0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff};

/* Tables from gzip's inflate.c (order of the bit length code lengths, copy lengths/distances). */
const unsigned int c_border[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
const unsigned short c_plens[31] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
const unsigned short c_plext[31] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99};
const unsigned short c_pdist[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577};
const unsigned short c_pdext[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
    12, 12, 13, 13};

/* CRC-32 table (gzip util.c); unsigned long is 64-bit on the EE */
const unsigned long crc_32_tab[256] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
    0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
    0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
    0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
    0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
    0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
    0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
    0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
    0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
    0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
    0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
    0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
    0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
    0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
    0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
    0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
    0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
    0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
    0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
    0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
    0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
    0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
    0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
    0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
    0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
    0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
    0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
    0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
    0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
    0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
    0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
    0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
    0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
    0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
    0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
    0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
    0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
    0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
    0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
    0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
    0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
    0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
    0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
    0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
    0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
    0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
    0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
    0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
    0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
    0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
    0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
    0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL,
};
