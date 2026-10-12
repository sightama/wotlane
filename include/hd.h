#ifndef HD_H
#define HD_H

#include "hieri_types.h"

/* Collision ("hd") types shared by common/hd, hdPoly, hdSphere, hdLos and the game's collision code. */

/* One contact. hit/dist/point/normal are in world space after hdTestNode/hdCsTest. */
struct _hdResult {
    unsigned char hit;      // 0x0
    unsigned char pad1;     // 0x1
    unsigned short surface; // 0x2 the hull's/poly object's surface word (offset 8 of the node)
    unsigned ids;           // 0x4 bits 0..10: node id2, bits 11..24: node id1 (hat id)
    float depth;            // 0x8 penetration depth (ContactInfo::depth)
    unsigned padC;          // 0xC
    FVECTOR point;          // 0x10 contact point (w: distance along the swept segment)
    FVECTOR normal;         // 0x20
};
typedef char _size__hdResult[sizeof(_hdResult) == 0x30 ? 1 : -1];

#define HD_RESULT_SET_IDS(r, id1, id2) ((r)->ids = ((r)->ids & ~0x1FFFFFF) | (((id1) & 0x3FFF) << 11) | ((id2) & 0x7FF))

struct ContactInfo {
    FVECTOR point;          // 0x0
    FVECTOR normal;         // 0x10
    float depth;            // 0x20
    unsigned pad[3];        // 0x24
};

/* A set of points of one cs tested against the hulls around it (hdCsTest). */
struct _hdCsTestPts {
    _cs *cs;                // 0x0
    int numPts;             // 0x4
    char done[0x68];        // 0x8 nonzero: skip that point
    FVECTOR pts[1];         // 0x70 in cs space
};

struct _hdSphereBundleTest {
    unsigned char pad0[0x10];
    _hdResult *results;     // 0x10
    int numResults;         // 0x14
};
typedef char _size__hdSphereBundleTest[sizeof(_hdSphereBundleTest) == 0x18 ? 1 : -1];

struct ColGridCsListNode {
    ColGridCsListNode *next;
    _cs *cs;
};
class ColGridCsList {
public:
    ColGridCsListNode *head;

    void pushCsFront(_cs *cs);
    void removeCs(_cs *cs);
};

/* Walks the hull nodes of the collision grid cells a moving sphere touches (common/HdIterator, asm). Only the fields
   hdCsTest reads are named. */
class HdItGridBox {
public:
    unsigned char pad0[0x30];
    int end;                // 0x30
    int pad34;              // 0x34
    int cur;                // 0x38
    unsigned char pad3C[0x10];
    unsigned skipId2;       // 0x4C node id2 of the tested cs (its own hulls are skipped)
    int skipSelf;           // 0x50
    int pad54;              // 0x54
    _hierhead *hull;        // 0x58 current hull node
    float (*mat)[4];        // 0x5C its world matrix
    unsigned short hatId1;  // 0x60 ids of the cs the hull belongs to
    unsigned short hatId2;  // 0x62
    unsigned char pad64[0xC];

    HdItGridBox();
    void reset(_fvector &from, _fvector &to, float r);
    void next(void);
} __attribute__((aligned(16)));
typedef char _size_HdItGridBox[sizeof(HdItGridBox) == 0x70 ? 1 : -1];

#endif
