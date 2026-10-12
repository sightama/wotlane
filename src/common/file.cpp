#include "common.h"
#include "sce_cdvd.h"

#define FILE_MAX_SLOTS 14

// Layout from the accessors below (retail size 0x184).
struct FileStatus {
    char *texAddr[FILE_MAX_SLOTS];        // 0x000
    signed char texLoaded;                // 0x038
    char *ngpAddr[FILE_MAX_SLOTS];        // 0x03C
    signed char ngpLoaded;                // 0x074
    char *resAddr[FILE_MAX_SLOTS];        // 0x078
    signed char resLoaded;                // 0x0B0
    char names[FILE_MAX_SLOTS][9];        // 0x0B1
    unsigned short maxTexId[FILE_MAX_SLOTS];   // 0x130
    unsigned short maxTexAddr[FILE_MAX_SLOTS]; // 0x14C
    unsigned short maxResAddr[FILE_MAX_SLOTS]; // 0x168
};
extern FileStatus fileStatus;
extern unsigned char cdSectorBuffer[];

struct sceCdRMode {
    unsigned char trycount, spindlctrl, datapattern, pad;
};
extern sceCdRMode cdReadMode;


struct sceCdlFILE {
    unsigned int lsn;
    unsigned int size;
    char name[16];
    unsigned char date[8];
    unsigned int flag;
};

/* Node of the CD directory tree built by fileMakeDirTree (root at cdFileSystemToc, 0x7000 bytes). A directory node
   is followed by room for 40 child pointers per sector of its ISO9660 directory record. */
struct _cdFileSystem {
    unsigned int sector;      /* 0x00 first sector (LSN) */
    unsigned short sectors;   /* 0x04 size in sectors */
    unsigned char type;       /* 0x06 2 = directory */
    unsigned char numEntries; /* 0x07 */
    int size;                 /* 0x08 size in bytes */
    char name[16];            /* 0x0C */
    _cdFileSystem *entries[1]; /* 0x1C numEntries children */
};
extern _cdFileSystem cdFileSystemToc;
extern int cdHasBeenInitialized;
extern char gFileName[];
extern char globalTimeString[];
extern char D_00735740[];
extern char D_006F3B68[]; /* "Couldn't find the file %s. TOC at %p\n" */
extern char D_006F3B90[]; /* "\t\tError #%i\n" */
extern int fileReadStatus __asm__("FileReadStatus.9"); /* static local of fileReada, starts at 1 */
extern char D_00735730[];  /* name of the file fileReada has in flight */
extern char D_006F8678[];  /* "" */
extern char D_006F8680[]; /* "Root\n" */
extern char D_006F8688[]; /* "  " */
extern char D_006F8690[]; /* " + %s" */
extern char D_006F8698[]; /* "  -%16s" */
extern char D_006F86A0[]; /* "host0:" */
extern char D_006F86A8[]; /* "cdrom0:" */
extern char D_006F86B0[]; /* "\\" */
extern char D_006F86B8[]; /* ";1" */

/* "\\" plus its terminator, copied as one 2-byte block like retail */
struct CharPair {
    char c[2];
};
extern "C" {
extern const char _ctype_[];
int strncmp(const char *a, const char *b, unsigned int n);
char *strncat(char *dst, const char *src, unsigned int n);
char *strcat(char *dst, const char *src);
int snd_StreamSafeCdSync(int);
int snd_StreamSafeCdGetError(void);
int snd_StreamSafeCdRead(int, int, void *, void *);
int sceCdGetError(void);
int sceSifInitRpc(unsigned int mode);
int sceSifRebootIop(const char *img);
int sceSifSyncIop(void);
int sceCdInit(int mode);
int sceCdMmode(int media);
int sceFsReset(void);
int sceCdDiskReady(int mode);
int strcmp(const char *a, const char *b);
char *strcpy(char *dst, const char *src);
unsigned int sceCdGetReadPos(void);
int sceCdRead(unsigned int lsn, unsigned int sectors, void *buf, sceCdRMode *mode);
int sceCdSync(int mode);
int sceOpen(const char *name, int flags, ...);
int sceWrite(int fd, const void *buf, int size);
int sceClose(int fd);
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
}
void setMaxTexId(int i, unsigned short v);
void setMaxTexAddr(int i, unsigned short v);
void setMaxResAddr(int i, unsigned short v);
int fileStringCompare(char *a, char *b);
void fileAdjustFileName(char *dst, char *src);
int fileCdSearchFile(sceCdlFILE *f, char *path);
void fileMakeDirTree(void);
int fourCharsToInt(int i);
int fileCdRead(long sectors, long lsn, char *buf);
_cdFileSystem *fileHierAddrOfSect(unsigned int sector);

__asm__("#SNFIX_SMALL cdHasBeenInitialized");
void fileInitializeCd(void)
{
    sceSifInitRpc(0);
    sceCdInit(0);
    while (!sceSifRebootIop("cdrom0:\\IOPRP24.IMG;1"))
        ;
    while (!sceSifSyncIop())
        ;
    sceSifInitRpc(0);
    sceCdInit(0);
    sceCdMmode(2);
    sceFsReset();
    while (sceCdDiskReady(0) != 2)
        ;
    fileMakeDirTree();
    cdHasBeenInitialized = 1;
    while (sceCdDiskReady(0) != 2)
        ;
}
int fileReadf(char *name, void *buf)
{
    sceCdlFILE f;
    unsigned int pos;
    int done;

    fileAdjustFileName(gFileName, name);
    printf("Reading %s into address %p\n", gFileName, buf);
    f.lsn = 0;
    f.size = 0;
    f.name[0] = 0;
    f.date[0] = 0;
    if (!fileCdSearchFile(&f, gFileName)) {
        printf(D_006F3B68, gFileName, &cdFileSystemToc);
        return 0;
    }
    snd_StreamSafeCdSync(0);
    do {
        if (snd_StreamSafeCdRead(f.lsn, (f.size + 0x7FF) >> 11, buf, &cdReadMode)) {
            pos = sceCdGetReadPos();
            while (snd_StreamSafeCdSync(0)) {
                if (sceCdGetReadPos() > pos + f.size / 10)
                    pos = sceCdGetReadPos();
            }
            done = 1;
            if (snd_StreamSafeCdGetError()) {
                done = 0;
                printf(D_006F3B90, snd_StreamSafeCdGetError());
            }
        } else
            done = 0;
    } while (!done);
    return f.size;
}
INCLUDE_ASM("asm/nonmatchings/common/file", D_006F3B68);
INCLUDE_ASM("asm/nonmatchings/common/file", D_006F3B90);
__asm__("#SNFIX_SMALL FileReadStatus.9");
int fileReada(char *name, void *buf)
{
    sceCdlFILE f;

    fileAdjustFileName(gFileName, name);
    printf("Trying to read %s into address %p - Asynchronous CD read\n", gFileName, buf);
    f.lsn = 0;
    f.size = 0;
    f.name[0] = 0;
    f.date[0] = 0;
    if (strcmp(D_00735730, gFileName) != 0) {
        if (!fileCdSearchFile(&f, gFileName)) {
            printf(D_006F3B68, gFileName, &cdFileSystemToc);
            return 0;
        }
        if (snd_StreamSafeCdRead(f.lsn, (f.size + 0x7FF) >> 11, buf, &cdReadMode)) {
            fileReadStatus = 0;
            strcpy(D_00735730, gFileName);
        } else
            printf("*-*=* FileRead Cmd Not Issued Properly to the IOP *=*-*\n");
        return fileReadStatus;
    }
    if (snd_StreamSafeCdSync(1))
        fileReadStatus = 0;
    else if (!snd_StreamSafeCdGetError()) {
        fileReadStatus = 1;
        D_00735730[0] = D_006F8678[0];
    } else
        printf(D_006F3B90, snd_StreamSafeCdGetError());
    return fileReadStatus;
}
#ifdef NON_MATCHING
/* 99/105 words: same code, retail swaps the registers of lsn and size+0x7FF (s4/s5) */
unsigned int fileReads(char *name, void *buf, unsigned int block)
{
    sceCdlFILE f;
    unsigned int size, lsn, pos;
    int done;

    fileAdjustFileName(gFileName, name);
    if (block == 0)
        printf("fileReads 512k block #%i of %s into address %p\n", block, gFileName, buf);
    f.lsn = 0;
    f.size = 0;
    f.name[0] = 0;
    f.date[0] = 0;
    if (!fileCdSearchFile(&f, gFileName)) {
        printf("Couldn't find the file %s.\n", gFileName);
        return 0;
    }
    size = 0x80000;
    if ((block + 1) << 19 >= f.size)
        size = f.size - (block << 19);
    lsn = f.lsn + (block << 8);
    sceCdSync(0);
    do {
        if (sceCdRead(lsn, (size + 0x7FF) >> 11, buf, &cdReadMode)) {
            pos = sceCdGetReadPos();
            while (sceCdSync(1)) {
                if (sceCdGetReadPos() > pos + size / 10)
                    pos = sceCdGetReadPos();
            }
            done = 1;
            if (sceCdGetError()) {
                done = 0;
                printf(D_006F3B90, sceCdGetError());
            }
        } else
            done = 0;
    } while (!done);
    return size;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/file", fileReads__FPcPvUi);
#endif
int fileWritef(char *name, void *buf, int size)
{
    int fd;

    fileAdjustFileName(gFileName, name);
    fd = sceOpen(gFileName, 0x602);
    if (fd < 0)
        return -2;
    sceWrite(fd, buf, size);
    sceClose(fd);
    return 0;
}
int getTexFilesLoaded(void)
{
    return fileStatus.texLoaded;
}
int getNgpFilesLoaded(void)
{
    return fileStatus.ngpLoaded;
}
int getResFilesLoaded(void)
{
    return fileStatus.resLoaded;
}
char *getNextNgpLoadAddr(void)
{
    return fileStatus.ngpAddr[fileStatus.ngpLoaded];
}
char *getNextTexLoadAddr(void)
{
    return fileStatus.texAddr[fileStatus.texLoaded];
}
char *getNextResLoadAddr(void)
{
    return fileStatus.resAddr[fileStatus.resLoaded];
}
char *getNgpAddr(int i)
{
    return i < fileStatus.ngpLoaded ? fileStatus.ngpAddr[i] : 0;
}
char *getTexAddr(int i)
{
    return i < fileStatus.texLoaded ? fileStatus.texAddr[i] : 0;
}
char *getResAddr(int i)
{
    return i < fileStatus.resLoaded ? fileStatus.resAddr[i] : 0;
}
char *getGenericName(int i)
{
    return i < fileStatus.ngpLoaded ? fileStatus.names[i] : 0;
}
int getMaxTexId(int i)
{
    return fileStatus.maxTexId[i];
}
int getMaxTexAddr(int i)
{
    return fileStatus.maxTexAddr[i];
}
int getMaxResAddr(int i)
{
    return fileStatus.maxResAddr[i];
}
void setMaxTexId(int i, unsigned short v)
{
    fileStatus.maxTexId[i] = v;
}
void setMaxTexAddr(int i, unsigned short v)
{
    fileStatus.maxTexAddr[i] = v;
}
void setMaxResAddr(int i, unsigned short v)
{
    fileStatus.maxResAddr[i] = v;
}
signed char getIdxOfAddr(char *addr)
{
    int i;

    for (i = 0; i < fileStatus.texLoaded; i++)
        if (fileStatus.texAddr[i] == addr)
            return i;
    return -1;
}
signed char getIdxOfResAddr(char *addr)
{
    int i;

    for (i = 0; i < fileStatus.resLoaded; i++)
        if (fileStatus.resAddr[i] == addr)
            return i;
    return -1;
}
signed char getIdxOfName(char *name)
{
    int i;

    for (i = 0; i < fileStatus.ngpLoaded; i++)
        if (fileStringCompare(name, fileStatus.names[i]))
            return i;
    return -1;
}
void fileAddNgpFile(char *addr, int size)
{
    int rem, pad;

    fileStatus.ngpAddr[fileStatus.ngpLoaded++] = addr;
    fileStatus.ngpAddr[fileStatus.ngpLoaded] = fileStatus.ngpAddr[fileStatus.ngpLoaded - 1] + size;
    rem = (int)fileStatus.ngpAddr[fileStatus.ngpLoaded] & 0x7F;
    if (rem) {
        pad = 0x80 - rem;
        fileStatus.ngpAddr[fileStatus.ngpLoaded] += (signed char)pad;
        fileStatus.ngpAddr[fileStatus.ngpLoaded] += 0x80;
    }
}
void fileAddTexFile(char *addr, int size)
{
    int rem, pad;

    fileStatus.texAddr[fileStatus.texLoaded++] = addr;
    fileStatus.texAddr[fileStatus.texLoaded] = fileStatus.texAddr[fileStatus.texLoaded - 1] + size;
    rem = (int)fileStatus.texAddr[fileStatus.texLoaded] & 0x7F;
    if (rem) {
        pad = 0x80 - rem;
        fileStatus.texAddr[fileStatus.texLoaded] += (signed char)pad;
        fileStatus.texAddr[fileStatus.texLoaded] += 0x70;
    }
}
void fileAddResFile(char *addr, int size)
{
    int rem, pad;

    setMaxResAddr(fileStatus.resLoaded, 0);
    setMaxTexAddr(fileStatus.resLoaded, 0);
    setMaxTexId(fileStatus.resLoaded, 0);
    fileStatus.resAddr[fileStatus.resLoaded++] = addr;
    fileStatus.resAddr[fileStatus.resLoaded] = fileStatus.resAddr[fileStatus.resLoaded - 1] + size;
    rem = (int)fileStatus.resAddr[fileStatus.resLoaded] & 0x7F;
    if (rem) {
        pad = 0x80 - rem;
        fileStatus.resAddr[fileStatus.resLoaded] += (signed char)pad;
        fileStatus.resAddr[fileStatus.resLoaded] += 0x70;
    }
}
void fileAddName(char *name)
{
    int i, j;

    for (i = 0; i < 9; i++)
        fileStatus.names[fileStatus.ngpLoaded - 1][i] = 0;
    for (j = 0; name[j] != 0 && j < 8; j++)
        fileStatus.names[fileStatus.ngpLoaded - 1][j] = name[j];
}
void fileOnlyNgpFile(char *addr, int size)
{
    char *end = addr + size;
    int rem, pad;

    fileStatus.ngpAddr[0] = addr;
    fileStatus.ngpLoaded = 1;
    fileStatus.ngpAddr[1] = end;
    rem = (int)end & 0x7F;
    if (rem) {
        pad = 0x80 - rem;
        fileStatus.ngpAddr[1] = end + (signed char)pad + 0x80;
    }
}
void fileOnlyTexFile(char *addr, int size)
{
    char *end = addr + size;
    int rem, pad;

    fileStatus.texAddr[0] = addr;
    fileStatus.texLoaded = 1;
    fileStatus.texAddr[1] = end;
    rem = (int)end & 0x7F;
    if (rem) {
        pad = 0x80 - rem;
        fileStatus.texAddr[1] = end + (signed char)pad + 0x70;
    }
}
void fileOnlyResFile(char *addr, int size)
{
    char *end = addr + size;
    int rem, pad;

    fileStatus.resAddr[0] = addr;
    fileStatus.resLoaded = 1;
    fileStatus.resAddr[1] = end;
    rem = (int)end & 0x7F;
    if (rem) {
        pad = 0x80 - rem;
        fileStatus.resAddr[1] = end + (signed char)pad + 0x70;
    }
}
void fileInitBeforeDbLoad(void)
{
    int i;

    for (i = 0; i < FILE_MAX_SLOTS; i++) {
        fileStatus.maxTexId[i] = 0;
        fileStatus.maxTexAddr[i] = 0;
        fileStatus.maxResAddr[i] = 0;
    }
}
void filePrintFileStatus(void)
{
}
#ifdef NON_MATCHING
/* equivalent, not tuned (size 0x438 vs 0x448): retail spills the queue indices and pointers to the stack */
void fileMakeDirTree(void)
{
    struct {
        long sector;
        long sectors;
    } queue[256];
    _cdFileSystem *node = &cdFileSystemToc;
    _cdFileSystem *parent;
    int sector, sectors;
    int tail = 0, head = 0, newEntry = 0;
    int i, j, off, child, filled, len;
    long extent;
    unsigned long size;

    cdReadMode.trycount = 20;
    cdReadMode.spindlctrl = 1;
    cdReadMode.datapattern = 0;
    for (i = 0; i < 0x810; i++)
        cdSectorBuffer[i] = 0;
    for (i = 0; i <= 0x7000; i++)
        ((char *)&cdFileSystemToc)[i] = 0;
    /* primary volume descriptor: root directory extent and size */
    fileCdRead(1, 16, (char *)cdSectorBuffer);
    sector = fourCharsToInt(0x9E);
    sectors = (fourCharsToInt(0xA6) + 0x7FF) / 0x800;
    queue[tail].sector = sector;
    queue[tail].sectors = sectors;
    tail++;
    while (head != tail) {
        parent = node;
        child = 0;
        sectors = queue[head].sectors;
        sector = queue[head].sector;
        head++;
        for (i = 0; i < sectors; i++) {
            fileCdRead(1, sector + i, (char *)cdSectorBuffer);
            for (off = 0; cdSectorBuffer[off] != 0; off += cdSectorBuffer[off]) {
                extent = fourCharsToInt(off + 2);
                size = fourCharsToInt(off + 10);
                filled = 0;
                if (cdSectorBuffer[off + 32] == 1 && cdSectorBuffer[off + 33] == 1) {
                    /* ".." */
                } else if (cdSectorBuffer[off + 32] == 1 && cdSectorBuffer[off + 33] == 0) {
                    /* "." describes the directory itself: the root fills its own node, other directories find theirs */
                    if (head < 2) {
                        filled = 1;
                        node->name[0] = '.';
                        node->name[1] = 0;
                    } else {
                        parent = fileHierAddrOfSect(sector);
                        if (!parent)
                            printf("fileMakeDirTree(%p); Shit, it didn't work.  I hit a directory I haven't cached.\n", &cdFileSystemToc);
                    }
                } else {
                    filled = 1;
                    newEntry = 1;
                    parent->entries[child++] = node;
                    parent->numEntries++;
                }
                if (filled) {
                    node->size = size;
                    node->sector = extent;
                    node->sectors = ((size - 1) >> 11) + 1;
                    node->type = cdSectorBuffer[off + 25];
                    if (node->type == 2 && newEntry) {
                        queue[tail].sector = extent;
                        queue[tail].sectors = (size + 0x7FF) >> 11;
                        newEntry = 0;
                        tail++;
                    }
                    len = cdSectorBuffer[off + 32];
                    for (j = 0; j < len; j++)
                        node->name[j] = cdSectorBuffer[off + 33 + j];
                    node->name[j] = 0;
                    if (node->type == 2) {
                        /* room for 40 child pointers per directory sector */
                        len = node->sectors;
                        for (j = 0; j < len; j++)
                            node = (_cdFileSystem *)((char *)node + 0xA0);
                    }
                    node = (_cdFileSystem *)((char *)node + 0x20);
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/file", fileMakeDirTree__Fv);
#endif
int fourCharsToInt(int i)
{
    return cdSectorBuffer[i] + (cdSectorBuffer[i + 1] << 8) + (cdSectorBuffer[i + 2] << 16) + (cdSectorBuffer[i + 3] << 24);
}
short twoCharsToShort(int i)
{
    return cdSectorBuffer[i] + (cdSectorBuffer[i + 1] << 8);
}
int fileCdRead(long sectors, long lsn, char *buf)
{
    int ret = sceCdRead(lsn, sectors, buf, &cdReadMode);

    sceCdSync(0);
    return ret;
}
#ifdef NON_MATCHING
/* equivalent, not tuned: retail keeps found on the stack and walks the path with two pointers; register allocation differs throughout */
int fileCdSearchFile(sceCdlFILE *f, char *path)
{
    _cdFileSystem *dir = &cdFileSystemToc;
    char dirName[16];
    char fileBuf[16];
    char *file = fileBuf;
    int found = 0;
    char *p, *q;
    int i, j, c, up;

    for (i = 0; path[i] != 0; i++) {
        c = path[i];
        up = c - 0x20;
        if (!((_ctype_ + 1)[c] & 2))
            up = c;
        path[i] = up;
    }
    if (path[0] != '\\')
        return 0;
    p = path + 1;
    q = dirName;
    while (*p != '\\' && *p != 0)
        *q++ = *p++;
    *q = 0;
    if (*p == '\\')
        file = p + 1;
    for (i = 0; i < dir->numEntries; i++) {
        if (fileStringCompare(dir->entries[i]->name, dirName)) {
            if (!(dir->entries[i]->type & 2)) {
                f->lsn = dir->entries[i]->sector;
                f->size = dir->entries[i]->size;
                for (j = 0; j < 8; j++)
                    f->date[j] = 0;
                for (j = 0; j < 16; j++)
                    f->name[j] = dir->entries[i]->name[j];
                return 1;
            }
            dir = dir->entries[i];
            found = 1;
        }
    }
    if (found) {
        for (i = 0; i < dir->numEntries; i++) {
            if (fileStringCompare(dir->entries[i]->name, file)) {
                f->lsn = dir->entries[i]->sector;
                f->size = dir->entries[i]->size;
                for (j = 0; j < 8; j++)
                    f->date[j] = 0;
                for (j = 0; j < 16; j++)
                    f->name[j] = dir->entries[i]->name[j];
                return 1;
            }
        }
    }
    printf("Could not find file %s, toc starts at %p\n", path, &cdFileSystemToc);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/file", fileCdSearchFile__FP10sceCdlFILEPc);
#endif
_cdFileSystem *fileHierAddrOfSect(unsigned int sector)
{
    int i;

    for (i = 0; i < cdFileSystemToc.numEntries; i++)
        if (cdFileSystemToc.entries[i]->sector == sector)
            return cdFileSystemToc.entries[i];
    return 0;
}
void filePrintCdFiles(_cdFileSystem *fs, int depth)
{
    int i, k;

    if (!fs)
        fs = &cdFileSystemToc;
    if (!depth)
        printf(D_006F8680);
    for (i = 0; i < fs->numEntries; i++) {
        for (k = 0; k < depth; k++)
            printf(D_006F8688);
        if (fs->entries[i]->type == 2) {
            printf(D_006F8690, fs->entries[i]->name);
            printf("<<DIR>>\n");
            filePrintCdFiles(fs->entries[i], depth + 1);
        } else {
            printf(D_006F8698, fs->entries[i]->name);
            printf("\tSEC=%6i\tSIZ=%9i\n", fs->entries[i]->sector, fs->entries[i]->sectors << 11);
        }
    }
}
int fileStringCompare(char *a, char *b)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (a[i] != b[i])
            return 0;
        if (a[i] == 0)
            return 1;
    }
    return 0;
}
#ifdef NON_MATCHING
/* 38/74 words: same logic; retail keeps two pointers for path[i] (test and copy), gcc merges them here */
char *fileTrimPath(char *path)
{
    int i, j;

    for (i = 0; i < 9; i++)
        D_00735740[i] = 0;
    i = 1;
    while (path[i] != '\\')
        i++;
    if (path[i] == '\\' || path[i] == '/' || path[i] == ':')
        i++;
    j = 0;
    while (path[i] != '.') {
        if (path[i] == '\\') {
            j = 0;
            i++;
        }
        D_00735740[j++] = path[i++];
    }
    D_00735740[j] = 0;
    printf("fileTrimPath passed in \"%s\", returning \"%s\"\n", path, D_00735740);
    return D_00735740;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/file", fileTrimPath__FPc);
#endif
char *fileGetTimeString(void)
{
    sceCdCLOCK clock;

    if (sceCdReadClock(&clock))
        sprintf(globalTimeString, "%02x:%02x:%02x", clock.hour, clock.minute, clock.second);
    else
        sprintf(globalTimeString, "Error Reading Time");
    return globalTimeString;
}
/* retail rodata keeps three empty strings after "Error Reading Time" */
__asm__(".section .rodata
	.word 0
	.word 0
	.word 0
	.text");
void fileAdjustFileName(char *dst, char *src)
{
    char tmp[16];
    int ci;

    if (strncmp(src, D_006F86A0, 6) == 0)
        src += 6;
    else if (strncmp(src, D_006F86A8, 7) == 0)
        src += 7;
    else if (*src == '/' || *src == '\\')
        src++;
    *(CharPair *)dst = *(CharPair *)D_006F86B0;
    while (*src) {
        switch (*src) {
        case ';':
            src++;
            break;
        case '/':
        case '\\':
            strncat(dst, D_006F86B0, 1);
            break;
        case '~':
            src++;
            break;
        default:
            ci = *src;
            {
                int up = ci - 0x20;
                if (!((_ctype_ + 1)[ci] & 2))
                    up = ci;
                tmp[0] = up;
            }
            strncat(dst, tmp, 1);
            break;
        }
        src++;
    }
    strcat(dst, D_006F86B8);
}
