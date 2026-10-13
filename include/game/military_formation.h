#ifndef MILITARY_FORMATION_H
#define MILITARY_FORMATION_H

class MilitaryPickup;

/* Group of military pickups (tanks, missile trucks) that move together behind a leader. vptr at 0x54, sizeof == 0x58. */
class MilitaryFormation {
public:
    MilitaryPickup *leader;       /* 0x00 */
    MilitaryPickup *members[20];  /* 0x04 */

    MilitaryFormation(void);
    virtual void setLeader(MilitaryPickup *p);
    virtual void addMember(MilitaryPickup *p);
    virtual void remove(MilitaryPickup *p);
    virtual void updatePositions(void) = 0;
    virtual void reverseFormation(void);
    virtual void initFormation(void);
};

/* Members in rows of `columns` (20 / columns rows), spaced by colSpacing and rowSpacing. sizeof == 0x64. */
class BoxFormation : public MilitaryFormation {
public:
    int columns;       /* 0x58 */
    float colSpacing;  /* 0x5C */
    float rowSpacing;  /* 0x60 */

    BoxFormation(int c, float cs, float rs) : columns(c), colSpacing(cs), rowSpacing(rs) {}
    virtual void setLeader(MilitaryPickup *p);
    virtual void addMember(MilitaryPickup *p);
    virtual void remove(MilitaryPickup *p);
    virtual void updatePositions(void);
    virtual void reverseFormation(void);
    virtual void initFormation(void);
};

#endif
