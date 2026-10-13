#include "common.h"

extern "C" {
int printf(const char *, ...);
int snd_StreamSafeCdSync(int);
int snd_StreamSafeCdGetError(void);
int snd_StreamSafeCdRead(int, int, void *, void *);
int sceCdStStop(void);
int sceCdStRead(int, void *, int, void *);
void sceSifFreeIopHeap(int);
}

struct StrFile {
    int size;
    char cdl[0x24];
    int iopHeap;
};

extern int partialMovie;
extern int nextSector;
extern int finalSector;
extern char mode[4];
extern int D_006F8DB0;

__asm__("#SNFIX_SMALL partialMovie");
__asm__("#SNFIX_SMALL nextSector");
__asm__("#SNFIX_SMALL finalSector");
__asm__("#SNFIX_SMALL D_006F8DB0");
INCLUDE_ASM("asm/nonmatchings/common/str", strFileOpen__FP7StrFilePc);
int strFileClose(StrFile *f)
{
    if (!partialMovie) {
        sceCdStStop();
        sceSifFreeIopHeap(f->iopHeap);
    }
    return 1;
}
int strFileRead(StrFile *f, void *buf, int bytes)
{
    int n;

    if (partialMovie) {
        int rem = finalSector - nextSector + 1;

        n = bytes >> 11;
        if (!((unsigned)bytes >> 11 < (unsigned)rem))
            n = rem;
        snd_StreamSafeCdSync(0);
        int err = snd_StreamSafeCdGetError();
        if (err)
            printf("ERROR in StreamSafeReads = %i\n", err);
        if (snd_StreamSafeCdRead(nextSector, n, buf, mode)) {
            nextSector += n;
            n <<= 11;
        } else {
            printf("Streaming Error!!!!!!!, snd_StreamSafeCdRead returned 0\n");
        }
    } else {
        n = sceCdStRead(bytes >> 11, buf, 1, &D_006F8DB0) << 11;
    }
    return n;
}
void strFileWait(int x)
{
    snd_StreamSafeCdSync(x);
}
/* retail rodata keeps an empty string plus alignment after the last literal of this TU */
__asm__(".section .rodata
	.word 0
	.text");
