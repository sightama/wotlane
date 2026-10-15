#ifndef HIT_EVENT_H
#define HIT_EVENT_H

/* Description of a hit handed to Monster::takeHit and to the states' acceptHit/isBlockable (the static one is
 * HitEvent::s_takeHitInfo). Only the fields seen so far are named; see Monster::takeHit for the type values. */
struct _fvector;
struct HitEvent {
    _fvector *pos; /* 0x00: where it lands (HomingBug: the contact point on a boss hull) */
    void *hd;      /* 0x04: the _hdResult of the contact (StateThrowBack::handleDamage) */
    char pad8[0xC - 0x8];
    int type;      /* 0x0C: 0 none, 1 normal (recoil), 2 blockable, 3 big hit, 5 shock, 7 stun, 8 knockback only, 10 burn */
    int source;    /* 0x10: what dealt it (0xB melee; 5, 6 and 0x14 are special-cased by StateBlock::isBlockable) */
    int subtype;   /* 0x14: recoil subtype for type 3 */
    char pad18[0x20 - 0x18];
    int sourceArg; /* 0x20: source detail (0x40 with source 5 = not blockable) */

    static HitEvent *create(void);
};

#endif
