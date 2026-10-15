#ifndef SOUND_OBJECTS_H
#define SOUND_OBJECTS_H

/* The small game/Sound objects that other headers embed by value (game/weapons.h); game/sound.h has the rest. */

struct _fvector;
class Weapon;

/* A positioned loop from any bank. */
class GenericSound {
public:
    unsigned m_handle;          // 0x0
    int m_bank;                 // 0x4 gSoundBanks index
    int m_sound;                // 0x8
    int m_minDist;              // 0xC
    int m_maxDist;              // 0x10
    void initGenericSound(int bank, int sound, int minDist, int maxDist);
    void terminateGenericSound(void);
    void updateGenericSound(_fvector *pos);
};

/* A projectile in flight: the flight loop of the weapon types that have one (with the doppler pitch in one-player games),
   plus the launch and explosion one-shots by weapon type. */
class WeaponSound {
public:
    unsigned m_handle;          // 0x0: flight loop
    float m_prevDist;           // 0x4: for the doppler pitch
    void initWeaponSound(void);
    void terminateWeaponSound(void);
    void updateWeaponSound(Weapon *w);
    void playWeaponLaunchSound(_fvector *pos, int type);
    void playWeaponExplosion(_fvector *pos, int type, int hit);
    void playRockSpecialImpact(_fvector *pos);
};

#endif
