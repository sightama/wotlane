#ifndef ENEMY_INFO_H
#define ENEMY_INFO_H

/* Pairwise table between the monster slots: s_info[i][j'] describes slot j as seen from slot i, where j' = j - 1 once j is past i
   (a slot has no entry for itself). Rebuilt every frame by EnemyInfo::update. */
class Monster;
class EnemyInfo {
public:
    struct Info {
        char pad0[0xC];
        float dist;       /* 0xC: distance */
        float dist2D;     /* 0x10: ground-plane distance */
        float dot;        /* 0x14: facing, compared with cos(angle) */
        float f18;        /* 0x18: compared with cos(45 degrees) by StateThrowBack (facing of the pair) */
        char pad1C[4];
        int los;          /* 0x20: clear line of sight */
        char pad24[0xC];
    };
    static Info s_info[16][15];

    static Info *info(int i, int j)
    {
        Info *row = s_info[i];

        return i < j ? &row[j - 1] : &row[j];
    }
    static Info *getInfo(Monster &a, Monster &b);
    static void init(void);
    static void update(void);
};

#endif
