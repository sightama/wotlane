#ifndef STREAMING_SOUND_H
#define STREAMING_SOUND_H

/* Music/ambience streaming; embedded in TheGame at 0x1204C0. */
class StreamingSoundManager {
public:
    char pad0[0x18];
    int f18;              /* 0x18: set by ShellSound::stopTokenSound */
    char pad1C[0x38 - 0x1C];
    int m_enabled;        /* 0x38: cleared by SoundManager::disableSoundForCinema in game modes 1-3 */
    char pad3C[0x109C - 0x3C];
    int m_paused;         /* 0x109C: all sounds paused (SoundManager::updatePauseAllSounds) */

    void updateStreamingSoundManager(int cinema);
    void bufferIntroCinemaMusic(void);
    void startCinemaMusic(void);
    void stopCinemaMusic(void);
};

#define STREAMING_SOUND() ((StreamingSoundManager *)((char *)game + 0x1204C0))

#endif
