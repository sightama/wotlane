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

/* Where the hull walk of one path or cs hierarchy stands (common/hdTraverse): the hull node it stopped at. */
struct hdTraverseState {
    _hierhead *hull;        // 0x0 current hull node
    float (*mat)[4];        // 0x4 its world matrix
    unsigned short hatId1;  // 0x8 ids of the cs the hull belongs to
    unsigned short hatId2;  // 0xA
    unsigned char padC[0xC];
};
typedef char _size_hdTraverseState[sizeof(hdTraverseState) == 0x18 ? 1 : -1];

/* The objects of one collision grid cell: its static paths, then (withCs) the cs objects registered in it. Each object is
   handed out once per walk: it is stamped with this walk's traceId. */
class HdItCell {
public:
    unsigned traceId;           // 0x0 from g_traceId, a new one per walk
    _HierListNode *path;        // 0x4 current path node (path->val: its bbox first), 0 when the paths are done
    ColGridCsListNode *csNode;  // 0x8 current cs node
    unsigned skipId2;           // 0xC node id2 of a cs to leave out (the tested cs itself)
    int withCs;                 // 0x10 walk the cs list too
    int triggers;               // 0x14 take the trigger paths instead of the solid ones (tdTest)

    HdItCell();
    void reset(int col, int row);
    void next(void);
    int isDone(void) { return !path && !csNode; }
};
typedef char _size_HdItCell[sizeof(HdItCell) == 0x18 ? 1 : -1];

/* Walks the hull nodes of the collision grid cells a moving sphere's bounding box touches, row by row. */
class HdItGridBox {
public:
    FVECTOR min;            // 0x0 box of the sweep, grown by the radius
    FVECTOR max;            // 0x10
    int pad20;              // 0x20
    int option;             // 0x24 passed to the hull walk
    int minRow;             // 0x28
    int minCol;             // 0x2C
    int maxRow;             // 0x30 exclusive
    int maxCol;             // 0x34 exclusive
    int row;                // 0x38
    int col;                // 0x3C
    HdItCell cell;          // 0x40
    hdTraverseState state;  // 0x58

    HdItGridBox();
    void reset(_fvector &from, _fvector &to, float r);
    int incrementPath(void);
    void next(void);
} __attribute__((aligned(16)));
typedef char _size_HdItGridBox[sizeof(HdItGridBox) == 0x70 ? 1 : -1];

/* The same along a line (hdLosTest): the cells are visited in the order the segment crosses them (a grid DDA). */
class HdItGridLine {
public:
    FVECTOR from;           // 0x0
    FVECTOR to;             // 0x10
    float r;                // 0x20
    int option;             // 0x24
    int pad28;              // 0x28
    float tEnter;           // 0x2C where the segment enters the grid
    float tExit;            // 0x30 where it leaves it; nearer hits than this end the walk early
    float tMaxX;            // 0x34 t of the next column boundary
    float tMaxY;            // 0x38 t of the next row boundary
    float tDeltaX;          // 0x3C t across one column
    float tDeltaY;          // 0x40
    float t;                // 0x44 t of the current cell
    int col;                // 0x48
    int row;                // 0x4C
    int stepX;              // 0x50 +-1
    int stepY;              // 0x54
    HdItCell cell;          // 0x58
    hdTraverseState state;  // 0x70
    unsigned char pad88[0x58];

    HdItGridLine();
    void reset(_fvector &from, _fvector &to, float r);
    int incrementPath(void);
    void next(void);
    int isDone(void) const;
    void drawGrid(void);
} __attribute__((aligned(16)));
typedef char _size_HdItGridLine[sizeof(HdItGridLine) == 0xE0 ? 1 : -1];

#endif
