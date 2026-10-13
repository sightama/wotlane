#ifndef SCE_CDVD_H
#define SCE_CDVD_H

/* libcdvd: the clock read from the CD/DVD controller (BCD fields) */
struct sceCdCLOCK {
    unsigned char stat, second, minute, hour, pad, day, month, year;
};

extern "C" int sceCdReadClock(sceCdCLOCK *clock);

#endif
