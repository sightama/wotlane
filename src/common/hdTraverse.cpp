#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "vecmath.h"

/* Hull walks for the collision grid iterators (common/HdIterator). A static path is a fixed list of nodes from the grid
   cell down to one hull; a cs hierarchy is walked depth-first with an explicit stack. Both keep the transform from world
   space into the hull's space on a matrix stack, and the ids of the nearest cs above the hull (its "hat"). */

/* One pending set of siblings on the walk stack, with the state that was current above them. */
struct hdTravFrame {
    _hierhead **kids;           // 0x0 next sibling to visit
    int left;                   // 0x4 siblings left
    float (*mat)[4];            // 0x8 matrix stack top
    unsigned short id2;         // 0xC
    unsigned short id1;         // 0xE
    _animCharInstance *charInst;  // 0x10 the character instance the animated nodes read their channels from
};
typedef char _size_hdTravFrame[sizeof(hdTravFrame) == 0x14 ? 1 : -1];

extern hdTravFrame hdTravCur __asm__("D_00765368");
extern hdTravFrame hdTravStack[] __asm__("D_007647B0");
extern QMat hdTravMats[] __asm__("D_00765380");
extern int hdTravDepth __asm__("D_006F8D50");
extern HierInteractiveState *hdTravInterState __asm__("D_006F8D54");

void mathfUnitMatrix(float (*m)[4]);
void mathfMulMatrix(float (*d)[4], float (*a)[4], float (*b)[4]);
void mathfMulMatrix3x3(float (*d)[4], float (*a)[4], float (*b)[4]);
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
extern "C" void hierStartRotMatPRH(FVECTOR4 *rot);
extern "C" void hierGetRotMatPRH(float (*m)[4]);
/* hand-written helpers in texm's assembly: are the node's channels dirty, and the follow-up for the skeleton */
extern "C" int func_0020803C(AnimationOutputBlock *out, _animtransform *xf);
extern "C" void func_00208074(_animCharInstance *ci, _animtransform *xf);

#ifdef NON_MATCHING
/* equivalent (untuned): 140/141 words, size 0x234 vs 0x234; retail reads the dirty/bits halfwords at 0xC0 with one ld */
/* Brings an animated transform node's matrix up to date from its channels (as hierAnimTransNode in common/hier).
   *outMat gets the matrix slot even when nothing changed. */
void updateAnimTransMatrix(_animtransform *xf, _animCharInstance *ci, float (**outMat)[4][4])
{
    AnimationOutputBlock *out = &ci->animOutput;
    float (*m)[4];
    float *val;

    if (xf->visidx >= 0 && ((out->dirty[xf->visidx / 8] >> (xf->visidx - xf->visidx / 8 * 8)) & 1))
        xf->visible = out->val[xf->visidx];
    if (xf->visible == 0.0f || !out)
        return;
    m = (float (*)[4])((char *)ci->animOutput.atMat + xf->matrixIdx * 64);
    if (outMat)
        *outMat = (float (*)[4][4])m;
    if (!func_0020803C(out, xf))
        return;
    val = out->val;
    if (xf->rXidx >= 0)
        xf->rotate.x = -val[xf->rXidx];
    if (xf->rYidx >= 0)
        xf->rotate.y = -val[xf->rYidx];
    if (xf->rZidx >= 0)
        xf->rotate.z = -val[xf->rZidx];
    hierStartRotMatPRH(&xf->rotate);
    if (xf->tXidx >= 0)
        xf->translate.x = val[xf->tXidx];
    if (xf->tYidx >= 0)
        xf->translate.y = val[xf->tYidx];
    if (xf->tZidx >= 0)
        xf->translate.z = val[xf->tZidx];
    hierGetRotMatPRH(m);
    if (xf->bits.applyPreRot) {
        if (xf->bits.applyPostRot) {
            QMat tmp;

            mathfMulMatrix(tmp.m, (float (*)[4])xf->preRot, m);
            mathfMulMatrix(m, tmp.m, (float (*)[4])xf->postRot);
        } else {
            mathfMulMatrix(m, (float (*)[4])xf->preRot, m);
        }
    } else if (xf->bits.applyPostRot) {
        mathfMulMatrix(m, m, (float (*)[4])xf->postRot);
    }
    vecAdd((_fvector *)m[3], (_fvector *)m[3], (_fvector *)&xf->translate);
    m[3][3] = 1.0f;
    func_00208074(ci, xf);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdTraverse", updateAnimTransMatrix__FP14_animtransformP17_animCharInstancePPA3_A3_f);
#endif

#ifdef NON_MATCHING
/* Pushes a matrix that undoes rotation rot (rows 0-2) about pivot rot[3] on top of the current one. */
static inline void pushInverseRot(float (*rot)[4])
{
    float (*old)[4] = hdTravCur.mat;
    float (*m)[4] = old + 4;
    FVECTOR t;

    hdTravCur.mat = m;
    mathfMulMatrix3x3(m, rot, old);
    vecSub(&t, (_fvector *)old[3], (_fvector *)rot[3]);
    mathfMulVec(rot, &t, (_fvector *)m[3]);
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 25/205 words, size 0x334 vs 0x32c */
/* Walks a path's nodes down to its hull, applying the transforms (only the switch/destructible branches the path goes
   through may be active, and a group can be masked out by option). Nonzero with st set when the hull is live. */
int hdTraversePath(_HierCollisionPath *path, hdTraverseState *st, unsigned short option)
{
    float (*mat)[4] = hdTravMats[0].m;
    float (*animMat)[4][4];
    _animCharInstance *charInst;
    HierInteractiveState *interState;
    unsigned short id1 = 1;
    unsigned short id2 = 0;
    int stop = 0;
    int i;

    mathfUnitMatrix(mat);
    charInst = 0;
    interState = 0;
    for (i = 0; i < path->length - 1 && !stop; i++) {
        _hierhead *node = path->path[i];

        if (node->opcode != CHARACTER_INSTANCE_NODE) {
            if (node->id1)
                id1 = node->id1;
            if (node->id2)
                id2 = node->id2;
        }
        switch (node->opcode) {
        case GROUP_NODE:
            if (((_hiergroup *)node)->flags & (option << 8))
                stop = 1;
            break;
        case TRANSLATE_NODE:
            vecSub((_fvector *)mat[3], (_fvector *)mat[3], (_fvector *)&((_hiertranslate *)node)->trans);
            break;
        case ROTATE_NODE: {
            float (*rot)[4] = (float (*)[4])((_hierrotate *)node)->mat;
            FVECTOR t;

            mat += 4;
            mathfMulMatrix3x3(mat, rot, mat - 4);
            vecSub(&t, (_fvector *)mat[-1], (_fvector *)rot[3]);
            mathfMulVec(rot, &t, (_fvector *)mat[3]);
            break;
        }
        case ANIM_TRANSFORM: {
            FVECTOR t;

            updateAnimTransMatrix((_animtransform *)node, charInst, &animMat);
            if (((_animtransform *)node)->visible == 0.0f) {
                stop = 1;
                break;
            }
            mat += 4;
            mathfMulMatrix3x3(mat, *animMat, mat - 4);
            vecSub(&t, (_fvector *)mat[-1], (_fvector *)(*animMat)[3]);
            mathfMulVec(*animMat, &t, (_fvector *)mat[3]);
            break;
        }
        case SWITCH_NODE:
            if (((_hierswitch *)node)->child[((_hierswitch *)node)->whichChild] != path->path[i + 1])
                stop = 1;
            break;
        case CHARACTER_INSTANCE_NODE:
            charInst = (_animCharInstance *)node;
            break;
        case INTERACTIVE_STATE_NODE:
            interState = (HierInteractiveState *)node;
            break;
        case DESTRUCTIBLE_NODE:
            if (*(_hierhead **)&((_hierdestructible *)node)->child[interState->whichChild] != path->path[i + 1])
                stop = 1;
            break;
        case NULL_NODE:
            stop = 1;
            break;
        }
    }
    if (stop) {
        st->hull = 0;
        return 0;
    }
    st->mat = mat;
    st->hull = path->path[i];
    st->hatId1 = st->hull->id1 ? st->hull->id1 : id1;
    st->hatId2 = st->hull->id2 ? st->hull->id2 : id2;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdTraverse", hdTraversePath__FP18_HierCollisionPathP15hdTraverseStateUs);
#endif

#ifdef NON_MATCHING
/* Saves the n siblings at kids for later, with the current matrix, ids and character instance. */
static inline void pushKids(_hierhead **kids, int n)
{
    hdTravFrame *f = &hdTravStack[hdTravDepth++];

    f->id2 = hdTravCur.id2;
    f->kids = kids;
    f->left = n;
    f->mat = hdTravCur.mat;
    f->charInst = hdTravCur.charInst;
    f->id1 = hdTravCur.id1;
}
/* The first of n children; the others are saved for later. */
static inline _hierhead *firstKid(_hierhead **kids, int n)
{
    if (n >= 2)
        pushKids(kids + 1, n - 1);
    return kids[0];
}
/* The next saved sibling (restoring the state above it), or 0 when the walk is over. */
static inline _hierhead *popKid(void)
{
    _hierhead *node = 0;
    int d = hdTravDepth;

    if (d > 0) {
        hdTravFrame *f = &hdTravStack[d - 1];

        node = *f->kids;
        hdTravCur.charInst = f->charInst;
        hdTravCur.id1 = f->id1;
        hdTravCur.id2 = f->id2;
        hdTravCur.mat = f->mat;
        if (--f->left > 0)
            f->kids++;
        else
            hdTravDepth = d - 1;
    } else {
        hdTravDepth = -1;
    }
    return node;
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 1/421 words, size 0x694 vs 0x5c0 */
/* Continues the depth-first walk of a cs hierarchy (set up by hdInitTraverseHier) to the next collision node: a hull, a
   bsp or a polygon object. Groups without the collision flag, or masked by option, are skipped. Zero when done. */
int hdTraverseHier(hdTraverseState *st, unsigned short option)
{
    _hierhead *node = popKid();

    while (hdTravDepth >= 0) {
        if (node->opcode != CHARACTER_INSTANCE_NODE) {
            if (node->id1)
                hdTravCur.id1 = node->id1;
            if (node->id2)
                hdTravCur.id2 = node->id2;
        }
        switch (node->opcode) {
        case GROUP_NODE: {
            _hiergroup *g = (_hiergroup *)node;

            if (!(g->flags & 2) || (g->flags & (option << 8)) || !g->numKids)
                break;
            node = firstKid(g->child, g->numKids);
            continue;
        }
        case LOD_NODE:
            node = ((_hierlod *)node)->lods[0].child;
            continue;
        case TRANSLATE_NODE: {
            _hiertranslate *tr = (_hiertranslate *)node;
            float (*old)[4] = hdTravCur.mat;

            hdTravCur.mat = old + 4;
            *(QMat *)hdTravCur.mat = *(QMat *)old;
            vecSub((_fvector *)hdTravCur.mat[3], (_fvector *)hdTravCur.mat[3], (_fvector *)&tr->trans);
            if (!tr->numKids)
                break;
            node = firstKid(tr->child, tr->numKids);
            continue;
        }
        case ROTATE_NODE: {
            _hierrotate *rot = (_hierrotate *)node;

            if (!rot->numKids)
                break;
            pushInverseRot((float (*)[4])rot->mat);
            node = firstKid(rot->child, rot->numKids);
            continue;
        }
        case SWITCH_NODE:
            node = ((_hierswitch *)node)->child[((_hierswitch *)node)->whichChild];
            continue;
        case CONTROL_NODE:
            pushKids(&((_hierctrl *)node)->child2, 1);
            node = ((_hierctrl *)node)->child1;
            continue;
        case COLLISION_VOLUME_NODE:
        case BSP_NODE:
        case POLY_COLLISION_OBJECT:
            st->hull = node;
            st->mat = hdTravCur.mat;
            st->hatId1 = hdTravCur.id1;
            st->hatId2 = hdTravCur.id2;
            return 1;
        case ANIM_TRANSFORM: {
            _animtransform *xf = (_animtransform *)node;
            float (*m)[4][4];

            if (!xf->bits.hasColl || !xf->numKids)
                break;
            updateAnimTransMatrix(xf, hdTravCur.charInst, &m);
            if (xf->visible == 0.0f)
                break;
            pushInverseRot(*m);
            node = firstKid(xf->child, xf->numKids);
            continue;
        }
        case CHARACTER_INSTANCE_NODE: {
            _animCharInstance *ci = (_animCharInstance *)node;

            if (!ci->numKids)
                break;
            hdTravCur.charInst = ci;
            node = firstKid(ci->child, ci->numKids);
            continue;
        }
        case DESTRUCTIBLE_NODE:
            node = *(_hierhead **)&((_hierdestructible *)node)->child[hdTravInterState->whichChild];
            continue;
        case SKELETON_BONE_NODE:
            node = ((_hierSkelBone *)node)->child;
            continue;
        case INTERACTIVE_STATE_NODE:
            hdTravInterState = (HierInteractiveState *)node;
            node = ((HierInteractiveState *)node)->child;
            continue;
        }
        node = popKid();
    }
    st->hull = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdTraverse", hdTraverseHier__FP15hdTraverseStateUs);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 4/34 words, size 0x88 vs 0x84 */
/* Starts a walk of the hierarchy at ep (a cs's entry point) with world-to-cs matrix m. */
void hdInitTraverseHier(float (*m)[4][4], _hierhead **ep)
{
    hdTravCur.id1 = 1;
    hdTravCur.charInst = 0;
    hdTravDepth = 0;
    hdTravCur.mat = hdTravMats[0].m;
    hdTravCur.id2 = 0;
    hdTravInterState = 0;
    hdTravMats[0] = *(QMat *)m;
    pushKids(ep, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdTraverse", hdInitTraverseHier__FPA3_A3_fPP9_hierhead);
#endif
