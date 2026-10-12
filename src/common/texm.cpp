#include "common.h"
#include "hieri_types.h"

/* Texture manager: keeps the level's textures in GS VRAM as a cache.
 *
 * A .TEX file is a header (`_texfile`: how many textures of each VRAM block size it has) followed by texture packets
 * (TexPkt: a descriptor qword, then a ready-made GS upload: DMA tag at +0x10, sceGsLoadImage at +0x20, pixels after).
 * texmAllocateBinSpace splits the VRAM between texmInit's base and lastVramTexAddr into bins, one per block size, with
 * slots proportional to how many textures need that size. A texture gets a slot (the least recently used one) when it
 * is drawn (texmGetTextureAddrs -> texmActivateTexture), and its upload packet is queued in the VU1 DMA chain.
 * texInfo[] (1000 entries) is the per-texture state; texId 0 is the frame buffer, 999 the movie surface.
 * .RTX textures ("resident") are uploaded once at load (texmLoadResTexture) below the cache. */

typedef unsigned long u_long;
/* GS DISPFB2 register (libgraph) */
struct tGS_DISPFB2 {
    u_long FBP : 9;
    u_long FBW : 6;
    u_long pad15 : 5;
    u_long PSM : 5;
    u_long pad25 : 7;
    u_long DBX : 11;
    u_long DBY : 11;
    u_long pad54 : 10;
};
struct sceGsLoadImage {
    u_long data[12];  // giftag, BITBLTBUF (+0x10), TRXPOS, TRXREG, TRXDIR, image giftag
};
extern "C" {
void sceGsSetDefLoadImage(sceGsLoadImage *lp, short dbp, short dbw, short dpsm, short x, short y, short w, short h);
int sceGsExecLoadImage(sceGsLoadImage *lp, u_long128 *src);
int sceGsSyncPath(int mode, unsigned short timeout);
void FlushCache(int op);
int printf(const char *, ...);
void *memset(void *, int, unsigned);
void exit(int);
}

/* one texture in a .TEX/.RTX file */
struct TexPkt {
    unsigned size;              // 0x0 (size >> 2) qwords to the next packet; 0 marks the last one
    unsigned pad4;              // 0x4
    unsigned short texId;       // 0x8
    unsigned short tbp;         // 0xA VRAM block (64 words) of resident textures
    unsigned psm : 6;           // 0xC GS pixel format
    unsigned noUpload : 1;      //     already set up, do not build the load image
    unsigned skip : 1;          //     not a cached texture
    unsigned logW : 4;
    unsigned logH : 4;
    unsigned numMips : 3;          // 0xE mip levels that follow (texIds texId+1..)
    unsigned unk19 : 4;
    unsigned unk23 : 4;
    unsigned bin : 5;           //     VRAM size bin
};
typedef char _size_TexPkt[sizeof(TexPkt) == 16 ? 1 : -1];

/* state of one texture (texInfo[1000] at 0x50F100) */
struct TexInfo {
    TexPkt *pkt;           // 0x0
    unsigned tbp : 14;     // 0x4 current VRAM block, valid while `resident`
    unsigned tbw : 6;      //     buffer width / 64
    unsigned bin : 5;
    unsigned numMips : 3;
    unsigned pad : 3;
    unsigned resident : 1; //     in VRAM
    int slot;              // 0x8 slot in its bin
    int pad2;              // 0xC
};
typedef char _size_TexInfo[sizeof(TexInfo) == 16 ? 1 : -1];

/* a .TEX header: how many textures there are of each block size */
struct _texfile {
    unsigned numSizes;     // 0x0
    unsigned headerQw;     // 0x4 qwords to the first packet
    struct {
        unsigned count;    // 0x8 + 8*i
        unsigned blkSize;  // 0xC + 8*i in bytes
    } sizes[1];
};
struct _texbins {
    unsigned count;        // slots in this bin
    unsigned size;         // block size
};
struct TexBinEntry {
    unsigned lastUsed;          // texLastUsedCnt when last drawn (0 = free)
    unsigned short texId;       // occupant
    unsigned short vramBlk;     // fixed VRAM block of the slot
};
enum _vramAddrs { VRAM_ADDR_A = 0x69880, VRAM_ADDR_B = 0x78840 };

extern TexInfo texInfo[1000];
extern TexBinEntry texBinE[][300];
extern _texbins *texBins;
extern _texbins texAllBins[15];
extern unsigned texLastUsedCnt;
extern unsigned lastVramTexAddr;
extern unsigned nextVramAddr;
__asm__("#SNFIX_SMALL nextVramAddr");
extern unsigned startVramAddr;
extern unsigned tempVramTexAddr;
extern int iBookmark;
extern int texmNumTextures;
extern int displayBuffer;
extern int movieBuffer;
extern int texBinStart[16] __asm__("D_00777FD0");    // first slot of each bin
extern _texfile *texFirstFile __asm__("D_006F8D58"); // texmRelocate: the level's .TEX
extern TexPkt *texLastPkt __asm__("D_006F8D5C");     // texmRelocate: end of the previous file
extern TexPkt *resLastPkt __asm__("D_006F8D60");      // texmResRelocate state
extern unsigned short resTexBlks __asm__("D_006F8D64");
extern unsigned short resResBlks __asm__("D_006F8D66");
extern unsigned resTexEnd __asm__("D_006F8D68");
extern unsigned resResEnd __asm__("D_006F8D6C");

char *getTexAddr(int i);
char *getResAddr(int i);
int getTexFilesLoaded(void);
int getResFilesLoaded(void);
signed char getIdxOfAddr(char *addr);
signed char getIdxOfResAddr(char *addr);
unsigned getMaxTexId(int i);
void setMaxTexId(int i, unsigned short v);
unsigned getMaxTexAddr(int i);
void setMaxTexAddr(int i, unsigned short v);
unsigned getMaxResAddr(int i);
void setMaxResAddr(int i, unsigned short v);
unsigned fptoui(float f) __asm__("fptoui");
void pktAddVu1Tex(Vu1DmaPkt *pkt, unsigned *tag);
int texmRelocate(char *file);
int texmResCheckForData(char *file);
int texmResRelocate(char *file, _vramAddrs base);
void texmLoadResTexture(QwData *file);
void texmLoadTexture(QwData *file);
void texmSetupTexture(int texId, int w, int h, int psm, int unk, QwData *pkt, int numMips, int bin);
void texmActivateTexture(int texId, QwData *tex0, Vu1DmaPkt *pkt);
unsigned short texmFindVramSpace(int texId);
int texmAllocateBinSpace(_texbins *bins);
int texmAlreadyGotBin(_texbins *bins, unsigned size, unsigned short *idx);
unsigned texmFindNextLargestTexSize(_texbins *bins, unsigned limit, unsigned short *idx);
int texmMatchingBlkSizeInFirstFile(unsigned size, _texfile *f);
void texmSetMipMapTbl(int texId, QwData *miptbl, Vu1DmaPkt *pkt);
void texmUpdateLastUsed(int texId);

#ifdef NON_MATCHING
static inline TexPkt *nextPkt(TexPkt *p) { return (TexPkt *)((char *)p + (p->size >> 2) * 16); }
/* PSMT8H / PSMT4HH / PSMT4HL: palettized textures stored in the high bits of a 32-bit buffer */
static inline int isHighPsm(unsigned psm) { return psm == 0x1B || psm == 0x2C || psm == 0x24; }
/* the TBP0 field of the BITBLTBUF in the packet's load image */
static inline void setUploadTbp(TexPkt *p, unsigned tbp)
{
    unsigned *dbp = (unsigned *)((char *)p + 0x34);  // high word of BITBLTBUF: DBP in bits 0..13
    *dbp = (*dbp & ~0x3FFF) | (tbp & 0x3FFF);
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 5/46 words, size 0xb0 vs 0xb8 */
void texmInit(_vramAddrs base)
{
    int i;
    switch (base) {
    case VRAM_ADDR_B:
        lastVramTexAddr = 0xFFFF0;
        break;
    case VRAM_ADDR_A:
        lastVramTexAddr = 0xEE7F0;
        break;
    default:
        lastVramTexAddr = 0xF97F0;
        break;
    }
    printf("Initing texm code, set lastVramTexAddr to %x\n", lastVramTexAddr);
    for (i = 0; i < getTexFilesLoaded(); i++)
        texmRelocate(getTexAddr(i));
    texLastUsedCnt = 0;
    texmLoadTexture((QwData *)getTexAddr(0));
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmInit__F10_vramAddrs);
#endif
/* relocates every .RTX that has textures, then uploads them from `base` up */
void texmResInit(_vramAddrs base, QwData *file)
{
    int i, any = 0;
    for (i = 0; i < getResFilesLoaded(); i++) {
        if (texmResCheckForData(getResAddr(i))) {
            any = 1;
            texmResRelocate(getResAddr(i), base);
        }
    }
    tempVramTexAddr = base;
    nextVramAddr = base;
    startVramAddr = base;
    if (any)
        texmLoadResTexture(file);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/198 words, size 0x314 vs 0x318 */
/* Uploads every resident texture now. Palettized "high" formats become resident texInfo entries; the others only
 * move the cache base (startVramAddr) above them, rounded to 8 blocks. */
void texmLoadResTexture(QwData *file)
{
    sceGsLoadImage li;
    unsigned counts[2];
    unsigned maxTbp;
    unsigned total = 0;
    TexPkt *p;
    unsigned size;

    memset(counts, 0, 8);
    maxTbp = 0;
    p = (TexPkt *)((char *)file + ((_texfile *)getResAddr(0))->headerQw * 16);
    do {
        int w = 1 << p->logW;
        int tbw = w < 0x41 ? 1 : w / 64;
        int dataQw;
        sceGsSetDefLoadImage(&li, (short)p->tbp, tbw, p->psm, 0, 0, 1 << p->logW, 1 << p->logH);
        FlushCache(0);
        dataQw = isHighPsm(p->psm) ? 8 : 1;
        sceGsSyncPath(0, 0);
        sceGsExecLoadImage(&li, (u_long128 *)p + dataQw);
        sceGsSyncPath(0, 0);
        if (isHighPsm(p->psm)) {
            TexInfo *t;
            *(sceGsLoadImage *)((char *)p + 0x20) = li;
            t = &texInfo[p->texId];
            t->pkt = p;
            t->tbp = p->tbp;
            t->tbw = tbw;
            t->numMips = p->numMips;
            t->resident = 1;
        } else {
            unsigned px = (1 << p->logW) << p->logH;
            if (px == 16)
                counts[0]++;
            else
                counts[1]++;
            if (px < 0x40)
                px = 0x40;
            total += px * 4;
            if (p->tbp >= maxTbp) {
                maxTbp = p->tbp;
                nextVramAddr = startVramAddr = px + (p->tbp << 6);
            }
        }
        size = p->size;
        p = nextPkt(p);
    } while (size);
    nextVramAddr = startVramAddr = startVramAddr + ((-(total >> 4) & 7) << 6);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmLoadResTexture__FP6QwData);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 24/172 words, size 0x2a8 vs 0x2b0 */
/* Sizes the bins, gives every bin slot its fixed VRAM block (first texture only) and fills texInfo for every cached
 * texture of `file`. */
void texmLoadTexture(QwData *file)
{
    _texfile *f0;
    TexPkt *p;
    int first = 1;
    int i;
    unsigned size;

    texmAllocateBinSpace(texAllBins);
    iBookmark = 0;
    for (i = 15; i >= 0; i--)
        texBinStart[i] = 0;
    f0 = (_texfile *)getTexAddr(0);
    texBins = texAllBins;
    p = (TexPkt *)((char *)file + f0->headerQw * 16);
    do {
        if (!p->skip) {
            if (first) {
                int b;
                first = 0;
                for (b = 0; b < (int)f0->numSizes; b++) {
                    int s;
                    for (s = 0; s < (int)texBins[b].count; s++) {
                        TexBinEntry *e = &texBinE[b + iBookmark][s + texBinStart[b]];
                        e->vramBlk = nextVramAddr >> 6;
                        e->lastUsed = 0;
                        e->texId = 0;
                        nextVramAddr += texBins[b].size >> 2;
                        if (lastVramTexAddr < nextVramAddr)
                            printf("Texture bin allocation overflow addr %x \n", nextVramAddr);
                    }
                    texBinStart[b] = s;
                }
                iBookmark = b;
                texmNumTextures = 0;
            }
            p->tbp = 0;
            if (++texmNumTextures == 1000)
                printf("Texture count at %i\n", 1000);
            texmSetupTexture(p->texId, 1 << p->logW, 1 << p->logH, p->psm, p->unk19, (QwData *)p, p->numMips, p->bin);
        }
        size = p->size;
        p = nextPkt(p);
    } while (size);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmLoadTexture__FP6QwData);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 12/75 words, size 0x124 vs 0x12c */
void texmSetupTexture(int texId, int w, int h, int psm, int, QwData *pkt, int numMips, int bin)
{
    TexInfo *t;
    int tbw = w < 0x41 ? 1 : w / 64;
    if (!((TexPkt *)pkt)->noUpload)
        sceGsSetDefLoadImage((sceGsLoadImage *)((char *)pkt + 0x20), 0, tbw, psm, 0, 0, w, h);
    t = &texInfo[texId];
    t->pkt = (TexPkt *)pkt;
    t->tbw = tbw;
    t->numMips = numMips;
    t->bin = bin;
    t->resident = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmSetupTexture__FiiiiiP6QwDataii);
#endif
void texmGetTextureDMATagAddress(int texId, unsigned **tag)
{
    *tag = (unsigned *)((char *)texInfo[texId].pkt + 0x10);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 6/7 words, size 0x1c vs 0x1c */
/* texId 0 draws from the display buffer */
void texmPrepTexInfoZero(tGS_DISPFB2 *fb)
{
    movieBuffer = 0;
    displayBuffer = fb->FBP << 5;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmPrepTexInfoZero__FP11tGS_DISPFB2);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 9/99 words, size 0x170 vs 0x18c */
/* Particle textures: fills tex0 (TBP0) and, when not resident, the DMA tag `tag` that uploads it. Returns 1 when an
 * upload was queued. */
int texmParticleTexture(int texId, QwData *tex0, QwData *tag)
{
    TexInfo *t;
    unsigned *w = (unsigned *)tag;
    unsigned short *tbp = (unsigned short *)tex0;
    if ((unsigned)texId >= 1000)
        return 0;
    if (texId == 0) {
        ((int *)tex0)[1] = 2;
        ((int *)tex0)[0] = displayBuffer | 0x28128000;
        return 0;
    }
    t = &texInfo[texId];
    texLastUsedCnt++;
    if (t->resident) {
        *tbp = (*tbp & 0xC000) | (((unsigned *)((char *)t->pkt + 0x30))[1] & 0x3FFF);
        return 0;
    }
    nextVramAddr = texmFindVramSpace(texId);
    setUploadTbp(t->pkt, nextVramAddr);
    t->tbp = nextVramAddr;
    w[0] = 0x50000000;   // DMA ref tag to the packet's upload (qwc in its own tag)
    w[3] = 0x11000000;
    w[2] = 0;
    w[1] = ((unsigned)t->pkt + 0x10) & 0x0FFFFFFF;
    t->resident = 1;
    *tbp = (*tbp & 0xC000) | (nextVramAddr & 0x3FFF);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmParticleTexture__FiP6QwDataT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 18/68 words, size 0x108 vs 0x110 */
/* gives texId a VRAM slot, queues its upload in `pkt` and patches TBP0 of the caller's TEX0 (and the mip table) */
void texmActivateTexture(int texId, QwData *tex0, Vu1DmaPkt *pkt)
{
    TexInfo *t = &texInfo[texId];
    unsigned short *tbp = (unsigned short *)tex0;
    texLastUsedCnt++;
    nextVramAddr = texmFindVramSpace(texId);
    setUploadTbp(t->pkt, nextVramAddr);
    t->tbp = nextVramAddr;
    pktAddVu1Tex(pkt, (unsigned *)((char *)t->pkt + 0x10));
    t->resident = 1;
    *tbp = (*tbp & 0xC000) | (nextVramAddr & 0x3FFF);
    if (t->numMips)
        texmSetMipMapTbl(texId, (QwData *)((char *)tex0 + 0x20), pkt);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmActivateTexture__FiP6QwDataP9Vu1DmaPkt);
#endif
/* uploads right away by DMA channel 2 (GIF) registers: hardware, stays asm */
INCLUDE_ASM("asm/nonmatchings/common/texm", texmActivateBitBlitTexture__FiP6QwData);
#ifdef NON_MATCHING
/* equivalent (untuned): 13/78 words, size 0x138 vs 0x128 */
/* least recently used slot of texId's bin (a never used slot first); evicts its occupant */
unsigned short texmFindVramSpace(int texId)
{
    unsigned bin = texInfo[texId].bin;
    int n = texBins[bin].count;
    unsigned best = 0;
    int slot = 0;
    int i = 0;
    TexBinEntry *e;
    if (n > 0 && texBinE[bin][0].lastUsed) {
        for (;;) {
            unsigned age = texLastUsedCnt - texBinE[bin][i].lastUsed;
            if (!(age < best)) {
                best = age;
                slot = i;
            }
            i++;
            if (i >= n)
                break;
            if (!texBinE[bin][i].lastUsed) {
                slot = i;
                break;
            }
        }
    }
    e = &texBinE[bin][slot];
    if (e->texId)
        texInfo[e->texId].resident = 0;
    texInfo[texId].slot = slot;
    e->lastUsed = texLastUsedCnt;
    e->texId = texId;
    return e->vramBlk;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmFindVramSpace__Fi);
#endif
int texmGetMipLevels(int texId)
{
    return texInfo[texId].numMips;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 49/269 words, size 0x414 vs 0x434 */
/* Slots per bin: every size gets at least one; the rest of the space is shared in proportion to the textures that
 * need each size, then any space left goes to the largest sizes first. */
int texmAllocateBinSpace(_texbins *bins)
{
    _texbins want[15];
    unsigned short idx;
    _texfile *f;
    int n = 0;
    int i;
    unsigned total = 0;
    unsigned used = 0;
    int avail;
    unsigned size;
    float ratio;

    for (i = 14; i >= 0; i--) {
        want[i].size = 0;
        bins[i].size = 0;
        bins[i].count = 0;
        want[i].count = 0;
    }
    f = (_texfile *)getTexAddr(0);
    for (i = 0; (unsigned)i < f->numSizes; i++) {
        if (texmAlreadyGotBin(want, f->sizes[i].blkSize, &idx)) {
            want[idx].count += f->sizes[i].count;
        } else {
            want[n].size = f->sizes[i].blkSize;
            bins[n].size = f->sizes[i].blkSize;
            want[n].count = f->sizes[i].count;
            n++;
        }
    }
    for (i = 0; i < n; i++)
        total += want[i].count * want[i].size;
    avail = (lastVramTexAddr - startVramAddr) * 4 - 0x10000;
    for (i = 0; i < n; i++) {
        if (!want[i].count)
            want[i].count = 1;
        avail -= bins[i].size;
    }
    ratio = (float)(unsigned)avail / (float)total;
    if (1.0f < ratio)
        ratio = 1.0f;
    for (i = 0; i < n; i++) {
        unsigned c = want[i].count;
        if (c < fptoui((float)c * ratio) + 1)
            bins[i].count = c;
        else
            bins[i].count = fptoui((float)c * ratio) + 1;
        used += (bins[i].count - 1) * bins[i].size;
    }
    avail += 0x10000;
    size = texmFindNextLargestTexSize(bins, 0, &idx);
    for (i = 0;;) {
        while (size < avail - used && size && bins[idx].count < want[idx].count) {
            bins[idx].count++;
            used += bins[idx].size;
        }
        size = texmFindNextLargestTexSize(bins, size, &idx);
        i++;
        if (!size || i >= 15)
            break;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmAllocateBinSpace__FP8_texbins);
#endif
int texmAlreadyGotBin(_texbins *bins, unsigned size, unsigned short *idx)
{
    int i;
    for (i = 0; i < 15; i++) {
        if (bins[i].size == size) {
            *idx = i;
            return 1;
        }
    }
    return 0;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/26 words, size 0x68 vs 0x48 */
/* largest bin size below `limit` (0 = no limit) */
unsigned texmFindNextLargestTexSize(_texbins *bins, unsigned limit, unsigned short *idx)
{
    unsigned best = 0;
    int i;
    for (i = 0; i < 15; i++) {
        if (bins[i].size < (limit ? limit : 0x100000) && best < bins[i].size) {
            *idx = i;
            best = bins[i].size;
        }
    }
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmFindNextLargestTexSize__FP8_texbinsUiPUs);
#endif
int texmMatchingBlkSizeInFirstFile(unsigned size, _texfile *f)
{
    unsigned i;
    for (i = 0; i < f->numSizes; i++)
        if (f->sizes[i].blkSize == size)
            return i;
    return -1;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 22/143 words, size 0x23c vs 0x228 */
/* Appends a .TEX loaded after the first one: its block sizes are merged into the first file's table (bin numbers in
 * the packets remapped), its texIds are moved past the previous file's, and the previous file's last packet is
 * linked to it. */
int texmRelocate(char *file)
{
    unsigned binMap[15];
    _texfile *f = (_texfile *)file;
    TexPkt *p;
    int idx, i;

    for (i = 14; i >= 0; i--)
        binMap[i] = 0x10;
    texFirstFile = (_texfile *)getTexAddr(0);
    p = (TexPkt *)(file + f->headerQw * 16);
    if (file != (char *)texFirstFile && f->numSizes) {
        unsigned k;
        for (k = 0; k < f->numSizes; k++) {
            int m = texmMatchingBlkSizeInFirstFile(f->sizes[k].blkSize, texFirstFile);
            if (m == -1) {
                binMap[k] = texFirstFile->numSizes;
                texFirstFile->sizes[texFirstFile->numSizes].blkSize = f->sizes[k].blkSize;
                texFirstFile->sizes[texFirstFile->numSizes].count = f->sizes[k].count;
                texFirstFile->numSizes++;
            } else {
                binMap[k] = m;
                texFirstFile->sizes[m].count += f->sizes[k].count;
            }
        }
    }
    idx = getIdxOfAddr(file);
    if (idx == 0) {
        TexPkt *q;
        do {
            if (getMaxTexId(idx) < p->texId)
                setMaxTexId(idx, p->texId);
            q = p;
            p = nextPkt(p);
        } while (q->size);
        texLastPkt = p;
    } else {
        TexPkt *q;
        texLastPkt->size = (((char *)p - (char *)texLastPkt) >> 4) * 4;
        do {
            p->texId += getMaxTexId(idx - 1);
            if (getMaxTexId(idx) < p->texId)
                setMaxTexId(idx, p->texId);
            p->bin = binMap[p->bin];
            q = p;
            p = nextPkt(p);
        } while (q->size);
        texLastPkt = p;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmRelocate__FPc);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/10 words, size 0x24 vs 0x28 */
/* does the .RTX hold any texture (the descriptor of its first packet is not empty) */
int texmResCheckForData(char *file)
{
    TexPkt *p = (TexPkt *)(file + ((_texfile *)file)->headerQw * 16);
    return (*(u_long *)&p->texId & ((u_long)0xFF00 << 32)) ? 1 : 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmResCheckForData__FPc);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 39/291 words, size 0x48c vs 0x468 */
/* Moves the textures of a .RTX to VRAM above `base` (and above the previous .RTX), tracking the highest block used
 * by the high-psm palettized ones (res) and by the others (tex), and links it after the previous file. */
int texmResRelocate(char *file, _vramAddrs base)
{
    TexPkt *p = (TexPkt *)(file + ((_texfile *)file)->headerQw * 16);
    int idx = getIdxOfResAddr(file);
    TexPkt *q;

    if (idx == 0) {
        setMaxTexAddr(0, 0);
        setMaxResAddr(0, 0);
        setMaxTexId(0, 0);
        do {
            if (!isHighPsm(p->psm)) {
                p->tbp += (int)base / 64;
                if (getMaxTexAddr(idx) < p->tbp) {
                    setMaxTexAddr(idx, p->tbp);
                    resTexBlks = (1 << p->logW) << p->logH;
                    if (resTexBlks < 0x40)
                        resTexBlks = 0x40;
                    resTexEnd = (resTexBlks >> 6) + getMaxTexAddr(idx);
                }
            } else if (p->tbp >= getMaxResAddr(idx)) {
                int px;
                setMaxResAddr(idx, p->tbp);
                px = 1 << p->logW;
                px = (px <= 0x3F ? 0x40 : px) << p->logH;
                resResBlks = px / 64;
                resResEnd = resResBlks + getMaxResAddr(idx);
            }
            if (getMaxTexId(idx) < p->texId)
                setMaxTexId(idx, p->texId);
            q = p;
            p = nextPkt(p);
        } while (q->size);
        resLastPkt = p;
        setMaxTexAddr(idx, resTexEnd);
        setMaxResAddr(idx, resResEnd);
    } else {
        int baseBlk = (int)base / 64;
        int prev = idx - 1;
        resLastPkt->size = (((char *)p - (char *)resLastPkt) >> 4) * 4;
        do {
            if (!isHighPsm(p->psm)) {
                p->tbp += baseBlk;
                p->tbp += getMaxTexAddr(prev) - baseBlk;
                if (getMaxTexAddr(idx) < p->tbp) {
                    setMaxTexAddr(idx, p->tbp);
                    resTexBlks = (1 << p->logW) << p->logH;
                    if (resTexBlks < 0x40)
                        resTexBlks = 0x40;
                    resTexEnd = (resTexBlks >> 6) + getMaxTexAddr(idx);
                }
            } else {
                p->tbp += getMaxResAddr(prev);
                if (getMaxResAddr(idx) < p->tbp) {
                    int px;
                    setMaxResAddr(idx, p->tbp);
                    px = 1 << p->logW;
                    px = (px <= 0x3F ? 0x40 : px) << p->logH;
                    resResBlks = px / 64;
                    resResEnd = resResBlks + getMaxResAddr(idx);
                    if ((unsigned)base < resResEnd) {
                        printf("TRYING TO LOAD TOO MUCH TEXTURE INTO SPACKLE TEXTURE LOCATION\n");
                        exit(0);
                    }
                }
                p->texId += getMaxTexId(prev);
                if (getMaxTexId(idx) < p->texId)
                    setMaxTexId(idx, p->texId);
            }
            q = p;
            p = nextPkt(p);
        } while (q->size);
        p->texId += getMaxTexId(prev);
        resLastPkt = p;
        setMaxResAddr(idx, resResEnd);
        setMaxTexAddr(idx, resTexEnd);
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmResRelocate__FPc10_vramAddrs);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/13 words, size 0x34 vs 0x34 */
/* the state hierTraverseAsm reads while drawing (saved into its cache block) */
void texmCacheForAsm(QwData *c)
{
    ((unsigned *)c)[0x11] = texLastUsedCnt;
    ((void **)c)[0x10] = texInfo;
    ((void **)c)[0x12] = texBinE;
    ((_texbins **)c)[0x1F] = texBins;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmCacheForAsm__FP6QwData);
#endif
void texmRestoreCache(QwData *c)
{
    texLastUsedCnt = ((unsigned *)c)[0x11];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/140 words, size 0x1e0 vs 0x230 */
/* Makes the mip levels of texId resident and writes MIPTBP1 (levels 1..3: TBP and TBW) into miptbl. */
void texmSetMipMapTbl(int texId, QwData *miptbl, Vu1DmaPkt *pkt)
{
    TexInfo *base = &texInfo[texId];
    TexInfo *m = base + 1;
    unsigned i;
    u_long v;
    int lv;
    for (i = 0; i < base->numMips; i++, m++) {
        if (!m->resident) {
            nextVramAddr = texmFindVramSpace(texId + 1 + i);
            setUploadTbp(m->pkt, nextVramAddr);
            m->tbp = nextVramAddr;
            m->resident = 1;
            pktAddVu1Tex(pkt, (unsigned *)((char *)m->pkt + 0x10));
        } else {
            texmUpdateLastUsed(texId + 1 + i);
        }
    }
    lv = texInfo[texId].numMips;
    v = texInfo[texId + 1].tbp | ((u_long)texInfo[texId + 1].tbw << 14);
    if (lv >= 2)
        v |= (u_long)texInfo[texId + 2].tbp << 20;
    if (lv >= 2)
        v |= (u_long)texInfo[texId + 2].tbw << 34;
    if (lv >= 3)
        v |= (u_long)texInfo[texId + 3].tbp << 40;
    if (lv >= 3)
        v |= (u_long)texInfo[texId + 3].tbw << 54;
    *(u_long *)miptbl = v;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmSetMipMapTbl__FiP6QwDataP9Vu1DmaPkt);
#endif
void texmUpdateLastUsed(int texId)
{
    texBinE[texInfo[texId].bin][texInfo[texId].slot].lastUsed = texLastUsedCnt;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 29/78 words, size 0x138 vs 0x134 */
/* Called from the draw code for every object texture: resident textures get their TBP0 patched and their slot
 * refreshed, the others are activated (upload queued). The high-psm palettized formats and texId 999 are left
 * alone; texId 0 is the display buffer. */
extern "C" void texmGetTextureAddrs(int texId, QwData *tex0, Vu1DmaPkt *pkt)
{
    unsigned psm = ((unsigned *)tex0)[0] & 0x03F00000;
    TexInfo *t;
    unsigned short *tbp = (unsigned short *)tex0;
    if (psm == 0x01B00000 || psm == 0x02C00000 || psm == 0x02400000)
        return;
    if (texId == 999)
        return;
    if (texId == 0) {
        *(u_long *)tex0 = (unsigned)(displayBuffer | 0x28128000) | ((u_long)1 << 33);  // TBP0 = display, TBW 10, TW/TH 10
        return;
    }
    t = &texInfo[texId];
    if (!t->resident) {
        texmActivateTexture(texId, tex0, pkt);
        return;
    }
    texBinE[t->bin][t->slot].lastUsed = ++texLastUsedCnt;
    *tbp = (*tbp & 0xC000) | (t->tbp & 0x3FFF);
    if (t->numMips)
        texmSetMipMapTbl(texId, (QwData *)((char *)tex0 + 0x20), pkt);
    else
        ((u_long *)tex0)[4] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/texm", texmGetTextureAddrs);
#endif
/* hand-written assembly (no C original): the scene traversal and packet builders used by the draw loop */
INCLUDE_ASM("asm/nonmatchings/common/texm", hierCsUpdateAsm);
INCLUDE_ASM("asm/nonmatchings/common/texm", hierTraverseAsm);
INCLUDE_ASM("asm/nonmatchings/common/texm", hierCacheForAsm);
INCLUDE_ASM("asm/nonmatchings/common/texm", hierCacheWorldMatsAsm);
INCLUDE_ASM("asm/nonmatchings/common/texm", pktAddVu1ObjAsm);
INCLUDE_ASM("asm/nonmatchings/common/texm", pktLinkAlphaObjAsm);
