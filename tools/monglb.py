#!/usr/bin/env python3
"""Exports a WotM monster to binary glTF (.glb) with skeleton and animations, for the web viewer (VIEWER_WEB.md, stage 1).

  python tools/monglb.py disc/MON/LIZARD0.NGP out/LIZARD0.glb               # idle (animation 90)
  python tools/monglb.py disc/MON/LIZARD0.NGP out/LIZARD0.glb --anims 90,0  # other animations (ids from pose.py --list)
  python tools/monglb.py disc/MON/LIZARD0.NGP out/LIZARD0.glb --all --split  # all of them: LIZARD0.glb (mesh, skeleton,
      idle and the list) + LIZARD0/aNNN.glb, one animation per file, loaded when the visitor picks it
      (--anim-dir LIZARD: writes to LIZARD/ next to it; a monster's costumes have identical animations and share the folder)
  python tools/monglb.py disc/LVL/3-MILE.NGP out/PLANT0.glb --boss plant --split --anim-dir PLANT   # bosses assembled
      from stage pieces: plant (3-MILE), root (3-MILE), mecha (CAPITOL); see boss()
  options: --unlit (unlit material; default: lit, like the native Renderer), --fps 60

What comes out (the same model as tools/pose.py, checked against RAM from a fight):
- A root node "WotM" rotates the game (Z-up) into glTF (Y-up). Everything inside it is in the game's units and axes.
- Each SKEL_BONE bone with an output (skelOutputMatIdx) becomes a joint; its parent is the bone that wrote the matrix it
  starts from on the skeleton stack (root = no parent). Inverse bind matrix = the bone's restPoseInv; skinned mesh positions
  = the file's (rest-pose model space), 3 bones per vertex with the game's weights.
- Rigid parts (unskinned OBJECTs under ANIM_XFORM) become nodes with the mesh in object space and the object matrix animated.
  Visibility (a visibility channel cutting off the node) comes out as scale 0 on the frames where the part disappears.
- Sampled animation: pose.py evaluates the curves every frame (60/s, 0..end) and each node gets a translation, rotation and
  scale key per frame (glTF interpolates linearly). Constant channels only keep the node's value.
- Effects (the viewer reproduces them like native/wotm_model.hpp): GS state per material in extras.gs (ALPHA_1, aref, zmsk);
  scrolling texture UVs in each animation's extras (extras.uv) and the 0x91 layer looping on the scene (extras.uvLayer);
  fire (emitter preset 141) and lights with lens flare as animated "fx_<node>_<n>" nodes, parameters in extras.fx.
- Left out: fur/tail springs, procedural look-at, blend layers. A skinned mesh with an object matrix (skin(v) * M, Agamo) or that disappears on some frames (Zorgulon
  costume 3): an "off_<obj>" node with M (scale 0 while hidden) and a copy of the bones.

Writes nothing from the game into the repo: it reads the user's disc and writes wherever the user says.
"""
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gltf  # noqa: E402
import ngp  # noqa: E402
import pose  # noqa: E402
import tex  # noqa: E402

ROOT_Q = [-math.sqrt(0.5), 0.0, 0.0, math.sqrt(0.5)]   # -90 degrees about X: (x, y, z) -> (x, z, -y), like gltf.to_y_up


def quat(R):
    """Quaternion (x, y, z, w) of a 3x3 rotation (column vectors)."""
    tr = R[0, 0] + R[1, 1] + R[2, 2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        q = ((R[2, 1] - R[1, 2]) / s, (R[0, 2] - R[2, 0]) / s, (R[1, 0] - R[0, 1]) / s, 0.25 * s)
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
        s = math.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2]) * 2
        q = (0.25 * s, (R[0, 1] + R[1, 0]) / s, (R[0, 2] + R[2, 0]) / s, (R[2, 1] - R[1, 2]) / s)
    elif R[1, 1] > R[2, 2]:
        s = math.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2]) * 2
        q = ((R[0, 1] + R[1, 0]) / s, 0.25 * s, (R[1, 2] + R[2, 1]) / s, (R[0, 2] - R[2, 0]) / s)
    else:
        s = math.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1]) * 2
        q = ((R[0, 2] + R[2, 0]) / s, (R[1, 2] + R[2, 1]) / s, 0.25 * s, (R[1, 0] - R[0, 1]) / s)
    n = math.sqrt(sum(c * c for c in q))
    return [c / n for c in q]


def trs(M):
    """Game matrix (row vectors, p' = p.M) -> (translation, quaternion, scale, decomposition error)."""
    A = M[:3, :3].T            # linear part as column vectors
    t = [float(x) for x in M[3, :3]]
    s = np.linalg.norm(A, axis=0)
    if np.any(s < 1e-9):
        return t, [0.0, 0.0, 0.0, 1.0], [0.0, 0.0, 0.0], 0.0
    R = A / s
    if np.linalg.det(R) < 0:
        s[0], R[:, 0] = -s[0], -R[:, 0]
    err = float(np.abs(R.T @ R - np.eye(3)).max())   # shear (non-uniform scale with rotation) doesn't fit in TRS
    return t, quat(R), [float(x) for x in s], err


class Track:
    """A node's keys: t/q/s per frame."""

    def __init__(self, n):
        self.t, self.q, self.s = [None] * n, [None] * n, [None] * n

    def put(self, f, M):
        t, q, s, err = trs(M)
        p = self.q[f - 1] if f > 0 else None
        if p is not None and sum(a * b for a, b in zip(p, q)) < 0:   # same hemisphere as the previous frame (shortest path)
            q = [-c for c in q]
        self.t[f], self.q[f], self.s[f] = t, q, s
        return err

    def fill(self, hidden=None):
        """Frames with no value: repeat the previous one (or the next); hidden = scale 0."""
        n = len(self.t)
        first = next((f for f in range(n) if self.t[f] is not None), None)
        for f in range(n):
            if self.t[f] is None:
                src = f - 1 if f > 0 else first
                if src is None:
                    self.t[f], self.q[f], self.s[f] = [0.0] * 3, [0.0, 0.0, 0.0, 1.0], [1.0] * 3
                else:
                    self.t[f], self.q[f], self.s[f] = self.t[src], self.q[src], self.s[src]
                if hidden:
                    self.s[f] = [0.0, 0.0, 0.0]


def const(vals, eps=1e-6):
    return all(max(abs(a - b) for a, b in zip(v, vals[0])) <= eps for v in vals)


class Exporter:
    def __init__(self, ngp_path, unlit=False, model=None, groups=None):
        """model: an already assembled pose.Model (bosses); groups: [(name, [instances], default id)]. A boss's animations
        are per piece: key k = 100 * group + index plays that animation in the group (on the group's other instances, the
        one with the same id) and the default-id animation in the other groups."""
        self.path = ngp_path
        self.mdl = model or pose.Model(ngp_path)
        self.groups = groups
        self.img = self.mdl.img   # base 0xA00000 (the monster .NGP comes prelinked)
        tp, rp = tex.sibling(ngp_path, ".TEX"), tex.sibling(ngp_path, ".RTX")
        self.lib = tex.Library(tp, rp, ngp_path) if (tp or rp) else None
        self.g = gltf.Glb()
        self.g.j["asset"]["generator"] = "WotM tools/monglb.py"
        self.stats = {"objetos": 0, "triangulos": 0, "skinned_fora": 0, "sprites_fora": 0, "sem_textura": 0, "texturas": 0,
                      "pesos_negativos": 0, "erro_trs": 0.0, "chaves": 0, "chaves_amostradas": 0}
        if unlit:
            self.g.j["extensionsUsed"] = ["KHR_materials_unlit"]
        self.mat = gltf.Materials(self.g, self.img, self.lib, unlit, self.stats).get

    # ---- mesh
    def primitives(self, node, md, joint_of=None):
        """Primitives per texture slot. joint_of: {palette slot: joint index} for a skinned mesh (None = rigid)."""
        g = self.g
        groups = {}
        for k in range(0, len(md.tris), 3):
            groups.setdefault(md.tris[k + 2].slot, []).append(md.tris[k:k + 3])
        prims = []
        for slot, tris in sorted(groups.items()):
            verts, index, idx = {}, [], []
            for tri in tris:
                for v in tri:
                    ln = math.sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz)
                    n = (v.nx / ln, v.ny / ln, v.nz / ln) if ln > 1e-6 else (0.0, 0.0, 1.0)
                    c = (min(1.0, v.r / 128.0), min(1.0, v.g / 128.0), min(1.0, v.b / 128.0), min(1.0, v.a / 128.0) if md.abe else 1.0)
                    jw = self.weights(v, joint_of) if joint_of is not None else None
                    key = ((v.x, v.y, v.z), n, (v.u, v.v), c, jw)
                    if key not in verts:
                        verts[key] = len(verts)
                        index.append(key)
                    idx.append(verts[key])
            attrs = {"POSITION": g.accessor("f", 3, [k[0] for k in index], minmax=True),
                     "NORMAL": g.accessor("f", 3, [k[1] for k in index]),
                     "COLOR_0": g.accessor("f", 4, [k[3] for k in index])}
            if slot != ngp.SLOT_NOTEX and self.lib is not None:
                attrs["TEXCOORD_0"] = g.accessor("f", 2, [k[2] for k in index])
            if joint_of is not None:
                attrs["JOINTS_0"] = g.accessor("H", 4, [k[4][0] for k in index], ctype=5123)
                attrs["WEIGHTS_0"] = g.accessor("f", 4, [k[4][1] for k in index])
            ctype, fmt = (5123, "H") if len(index) < 65536 else (5125, "I")
            prims.append({"attributes": attrs, "indices": g.accessor(fmt, 1, idx, ctype=ctype, target=34963),
                          "material": self.mat(node, slot, md.abe, self.gs_of.get(node))})
            self.stats["triangulos"] += len(tris)
        return prims

    def weights(self, v, joint_of):
        """The vertex's 3 bones (Scene::skinVertex: w1 = w, w2 = frac(w*2048), w3 = the remainder) -> (joints[4], weights[4])."""
        w1 = v.w
        s = v.w * 2048.0
        w2 = s - math.trunc(s)
        acc = {}
        for code, w in ((v.cx, w1), (v.cy, w2), (v.cz, 1.0 - w1 - w2)):
            if w == 0.0:
                continue
            if w < 0.0:   # encoding rounding makes w3 as low as -0.00025 (w1 + w2 goes past 1): clamp to zero
                if w < -1e-3:
                    self.stats["pesos_negativos"] += 1
                continue
            j = joint_of.get((code & 0x7F) // 4, joint_of["fixo"])
            acc[j] = acc.get(j, 0.0) + w
        tot = sum(acc.values()) or 1.0
        js = sorted(acc.items(), key=lambda kv: -kv[1])[:4]
        js += [(0, 0.0)] * (4 - len(js))
        return tuple(j for j, _ in js), tuple(w / tot for _, w in js)

    # ---- animation
    def sample(self, k, fps):
        """Poses of animation index k at every frame (60/s, 0..end) -> (id, poses, times)."""
        if self.groups:
            gi, kk = divmod(k, 100)
            name, cis, _d = self.groups[gi]
            a = self.mdl.find_anim(None, ci=cis[0], k=kk)
            if a is None:
                raise ValueError("%s: %s has no animation %d" % (self.path, name, kk))
            aid, _k, st, et, _nch = a

            def by_id(ci, want, fallback):
                # the game looks animations up by id (animationGetHandle: head.id1 == animId), not by table position,
                # and the plant's heads don't have their tables in the same order
                return next((x[1] for x in self.mdl.anims(ci) if x[0] == want), fallback)

            assign = {}
            for gj, (_n, cs, did) in enumerate(self.groups):
                for ci in cs:
                    assign[ci] = by_id(ci, aid, kk) if gj == gi else by_id(ci, did, 0)
            n = max(1, int(math.floor((et - st) * fps / 60.0 + 1e-6)) + 1)
            frames = [self.mdl.pose(frame=f * 60.0 / fps, assign=assign) for f in range(n)]
            return aid, frames, [f / fps for f in range(n)]
        a = self.mdl.find_anim(None, k=k)
        if a is None:
            raise ValueError("%s: no animation k %d" % (self.path, k))
        aid, _k, st, et, _nch = a
        n = max(1, int(math.floor((et - st) * fps / 60.0 + 1e-6)) + 1)
        frames = [self.mdl.pose(aid, f * 60.0 / fps, k=k) for f in range(n)]
        return aid, frames, [f / fps for f in range(n)]

    def export(self, out, keys, fps=60.0, split=False, anim_dir=None, uv_layer=None):
        """keys: animation indexes k (the first one gives the nodes' initial values). split=True: the main .glb carries only
        the first animation and the list of all of them; each animation goes into its own .glb at <out without .glb>/aNNN.glb,
        with the same node names (the viewer loads it when the visitor picks it). anim_dir: a different folder (next to out)
        for those animations; a monster's costumes produce byte-identical animations, so they can share a folder
        (--anim-dir LIZARD)."""
        g, img = self.g, self.img
        fx_kind = {}

        def frame_from(normal):
            """mathfMatrixFromNormal: rows X, n, Z (row vectors) with n in row 1."""
            nl = np.array(normal, dtype=float)
            ln = np.linalg.norm(nl)
            if ln < 1e-9:
                return np.eye(3)
            nl /= ln
            if abs(nl[2]) > 0.999:
                a0 = np.array([1.0, 0.0, 0.0])
            else:
                h = math.hypot(nl[0], nl[1])
                a0 = np.array([nl[1] / h, -nl[0] / h, 0.0])
            return np.array([a0, nl, np.cross(a0, nl)])

        def fx_items(items):
            """[((node, occurrence), effect matrix in the monster's world)] for the effects visited in this pose."""
            out, occ = [], {}
            for kind, lst in (("fire", items.emitters), ("flare", items.lights)):
                for n, m in lst:
                    if kind == "fire":
                        preset = img.u32(n + 4)
                        if preset != 141:   # only IdolHeadFlame (Agamo, Magmo costume 4) stays visible and is simulated
                            continue
                        h, pt = img.f32(n + 0x24), img.f32(n + 0x28)
                        R = frame_from([math.cos(pt) * math.sin(h), math.cos(pt) * math.cos(h), math.sin(pt)])
                        info = {"type": "fire", "preset": preset}
                    else:
                        fl = img.u32(n + 4)
                        if not (fl & 1) or not (fl & 2):   # isOn, hasLensFlare
                            continue
                        R = frame_from([img.f32(n + 0x20), img.f32(n + 0x24), img.f32(n + 0x28)])   # row 1 = beam
                        info = {"type": "flare", "flags": fl, "intensity": img.f32(n + 8), "beamWidth": img.f32(n + 0x3C),
                                "rgb": [img.f32(n + 0x30 + 4 * c) for c in range(3)]}
                    L = np.eye(4)
                    L[:3, :3] = R
                    L[3, :3] = [img.f32(n + 0x10), img.f32(n + 0x14), img.f32(n + 0x18)]
                    o = occ.get(n, 0)
                    occ[n] = o + 1
                    fx_kind.setdefault((n, o), info)
                    out.append(((n, o), L @ m))
            return out

        sampled = []
        for k in keys:
            aid, frames, times = self.sample(k, fps)
            sampled.append((k, aid, frames, times))

        # structure: bones (per instance) and objects, from the union of the frames of every animation
        bones = {}      # (ci, output) -> (SKEL_BONE node, parent (ci, output) or None)
        skinned = {}    # (OBJECT, ci) -> palette slots {slot: output}
        rigid = []      # (OBJECT, occurrence) in the order they appear
        fx = []         # (op 22/5 node, occurrence) of an effect: fire (preset 141) and light with lens flare
        sig = {}        # skinned (OBJECT, ci) -> {global frame: object matrix}
        nframes = 0
        self.gs_of = {}   # OBJECT -> inherited GS state (first one seen; it comes from the nodes' ctx, so the pose doesn't change it)
        for _k, _aid, frames, _times in sampled:
            for items in frames:
                gf = nframes
                nframes += 1
                seen = {}
                for it in items:
                    node, m, ctx = it
                    self.gs_of.setdefault(node, it.gs)
                    md_skinned = ngp.decode_mesh(img, node).skinned
                    if md_skinned:
                        if ctx is None:
                            continue
                        key = (node, ctx.ci)
                        sig.setdefault(key, {})[gf] = m
                        if key not in skinned:
                            skinned[key] = self.mdl.palette_outs(node, ctx) or {}
                        for o, (bn, _S, par) in ctx.bones.items():
                            bones.setdefault((ctx.ci, o), (bn, (ctx.ci, par) if par is not None else None))
                    else:
                        occ = seen.get(node, 0)
                        seen[node] = occ + 1
                        if (node, occ) not in rigid:
                            rigid.append((node, occ))
                for key, _m in fx_items(items):
                    if key not in fx:
                        fx.append(key)

        root = {"name": "WotM", "rotation": ROOT_Q, "children": []}
        g.j["nodes"].append(root)
        g.j["scenes"][0]["nodes"].append(0)

        def add_node(d, parent):
            g.j["nodes"].append(d)
            i = len(g.j["nodes"]) - 1
            g.j["nodes"][parent].setdefault("children", []).append(i)
            return i

        # unique node names: the tracks of animations in their own files find the node by name
        one_ci = len({b[0] for b in bones}) <= 1

        def bone_name(b):
            return "bone%d" % b[1] if one_ci else "bone%x_%d" % b

        # bones in parent-before-child order
        bone_node = {}
        pending = sorted(bones)
        while pending:
            rest = []
            for b in pending:
                par = bones[b][1]
                if par is None or par not in bones:
                    bone_node[b] = add_node({"name": bone_name(b)}, 0)
                elif par in bone_node:
                    bone_node[b] = add_node({"name": bone_name(b)}, bone_node[par])
                else:
                    rest.append(b)
            if len(rest) == len(pending):   # cycle (shouldn't happen): hang them off the root
                for b in rest:
                    bone_node[b] = add_node({"name": bone_name(b)}, 0)
                rest = []
            pending = rest

        # Skinned mesh with an object matrix M (the game computes skin(v) * M, e.g. Agamo) or that disappears on some frames
        # (e.g. Zorgulon costume 3): glTF ignores the node matrix of a skinned mesh, so M becomes an "off_<obj>" node
        # (animated like a rigid part, scale 0 on frames without the mesh) with a copy of the instance's bones under it
        # (same tracks); the mesh's skin uses the copy. Meshes with the same M on every frame share the node.
        eye = np.eye(4)
        groups = {}
        for key, ms in sig.items():
            if len(ms) == nframes and all(np.allclose(m, eye, atol=1e-5) for m in ms.values()):
                continue   # always present and no matrix: a normal skin
            fp = tuple((gf, np.round(ms[gf], 4).tobytes() if gf in ms else None) for gf in range(nframes))
            groups.setdefault((key[1], fp), []).append(key)
        off_of = {}          # (OBJECT, ci) -> offset node
        dup_bones = {}       # offset node -> {bone: copy}
        for (ci, _fp), keys in sorted(groups.items(), key=lambda kv: min(kv[1])):
            tag = min(k[0] for k in keys)
            on = add_node({"name": "off_%x" % tag}, 0)
            dup = {}
            for b in bone_node:   # creation order: parent before child
                if b[0] != ci:
                    continue
                par = bones[b][1]
                dup[b] = add_node({"name": "%s@%x" % (bone_name(b), tag)}, dup[par] if par in dup else on)
            dup_bones[on] = dup
            for k in keys:
                off_of[k] = on

        # skins: one per instance, with all of its bones + a fixed joint (identity) for slots with no bone
        skins = {}
        skin_mesh_node = {}   # (OBJECT, ci) -> mesh node
        for (node, ci), outs in skinned.items():
            sk = off_of.get((node, ci), ci)
            if sk not in skins:
                js = [b for b in sorted(bones) if b[0] == ci]
                if sk == ci:
                    fixo = add_node({"name": "static_%x" % ci}, 0)
                    joints = [bone_node[b] for b in js] + [fixo]
                else:   # the offset node itself is the fixed joint (world = M)
                    fixo = sk
                    joints = [dup_bones[sk][b] for b in js] + [fixo]
                ibm = []
                for b in js:
                    rp = np.array(struct.unpack_from("<16f", img.d, bones[b][0] + 0x10 - img.base)).reshape(4, 4)
                    ibm.append(rp.T.flatten(order="F"))   # restPoseInv as column vectors, column by column
                ibm.append(np.eye(4).flatten(order="F"))
                data = struct.pack("<%df" % (16 * len(ibm)), *[float(x) for m in ibm for x in m])
                g.j["accessors"].append({"bufferView": g.view(data), "componentType": 5126, "count": len(ibm), "type": "MAT4"})
                g.j["skins"] = g.j.get("skins", []) + [{"joints": joints, "inverseBindMatrices": len(g.j["accessors"]) - 1,
                                                         "skeleton": 0, "name": "skeleton_%x" % ci if sk == ci else
                                                         "skeleton_%x@%s" % (ci, g.j["nodes"][sk]["name"][4:])}]
                skins[sk] = (len(g.j["skins"]) - 1, {b[1]: k for k, b in enumerate(js)}, len(joints) - 1)
            si, jidx, fixo = skins[sk]
            joint_of = {s: jidx[o] for s, o in outs.items() if o in jidx}
            joint_of["fixo"] = fixo
            md = ngp.decode_mesh(img, node)
            self.stats["sprites_fora"] += len(md.sprites)
            if not md.tris:
                continue
            g.j["meshes"].append({"name": "obj_%x" % node, "primitives": self.primitives(node, md, joint_of)})
            g.j["nodes"].append({"name": "obj_%x" % node, "mesh": len(g.j["meshes"]) - 1, "skin": si})
            g.j["scenes"][0]["nodes"].append(len(g.j["nodes"]) - 1)
            skin_mesh_node[(node, ci)] = len(g.j["nodes"]) - 1
            self.stats["objetos"] += 1

        rigid_node = {}
        for node, occ in rigid:
            md = ngp.decode_mesh(img, node)
            self.stats["sprites_fora"] += len(md.sprites)
            if not md.tris:
                continue
            g.j["meshes"].append({"name": "obj_%x" % node, "primitives": self.primitives(node, md)})
            rigid_node[(node, occ)] = add_node({"name": "part_%x_%d" % (node, occ), "mesh": len(g.j["meshes"]) - 1}, 0)
            self.stats["objetos"] += 1

        names = [nd["name"] for nd in g.j["nodes"]]
        if len(set(names)) != len(names):
            raise ValueError("duplicate node names: animations in their own files would not find the node")

        rigid_objs = {k[0] for k in rigid_node}

        # Effects: one "fx_<node>_<occurrence>" node per fire emitter / light with lens flare, animated with the effect's matrix
        # (the parent's, with the emitter's basis or the light's beam on top) and scale 0 on frames where the node isn't
        # visited. The fixed parameters go in extras.fx; the viewer simulates the fire and draws the flare like
        # native/wotm_model.hpp.
        fx_node = {}
        for n, occ in fx:
            if (n, occ) in fx_kind:
                fx_node[(n, occ)] = add_node({"name": "fx_%x_%d" % (n, occ), "extras": {"fx": fx_kind[(n, occ)]}}, 0)

        def scrolls(node):
            return bool(img.u16(node + 0xC) & 0x800)   # uvScroll: the UV moves with the channels at +0x30/+0x32

        def tracks_of(frames):
            """TRS tracks per node and the UV offset per mesh node with uvScroll ({node: [(u, v) per frame]})."""
            n = len(frames)
            tracks = {}
            uvs = {}
            hidden = set()   # parts that disappear on some frame (scale 0 on those frames)
            for f, items in enumerate(frames):
                ctxs = {}
                seen = {}
                present = set()
                present_off = set()
                for it in items:
                    node, m, ctx = it
                    if scrolls(node):
                        ni = skin_mesh_node.get((node, ctx.ci)) if ctx is not None else None
                        if ni is None:
                            ni = rigid_node.get((node, seen.get(node, 0)))
                        if ni is not None:
                            uvs.setdefault(ni, [None] * n)[f] = it.uv
                    if ctx is not None:
                        ctxs[ctx.ci] = ctx
                        on = off_of.get((node, ctx.ci))
                        if on is not None and on not in present_off:
                            present_off.add(on)
                            err = tracks.setdefault(on, Track(n)).put(f, m)
                            self.stats["erro_trs"] = max(self.stats["erro_trs"], err)
                    if node in rigid_objs:
                        occ = seen.get(node, 0)
                        seen[node] = occ + 1
                        key = (node, occ)
                        if key in rigid_node:
                            err = tracks.setdefault(rigid_node[key], Track(n)).put(f, m)
                            self.stats["erro_trs"] = max(self.stats["erro_trs"], err)
                            present.add(key)
                for key, M in fx_items(items):
                    if key in fx_node:
                        tracks.setdefault(fx_node[key], Track(n)).put(f, M)
                        present.add(key)
                for key, ni in fx_node.items():
                    if key not in present:
                        tracks.setdefault(ni, Track(n))
                        hidden.add(ni)
                for key, ni in rigid_node.items():
                    if key not in present:
                        tracks.setdefault(ni, Track(n))
                        hidden.add(ni)
                for on in dup_bones:
                    if on not in present_off:
                        tracks.setdefault(on, Track(n))
                        hidden.add(on)
                for b, ni in bone_node.items():
                    ctx = ctxs.get(b[0])
                    if ctx is None or b[1] not in ctx.bones:
                        continue
                    _bn, S, par = ctx.bones[b[1]]
                    L = S
                    if par is not None and par in ctx.bones and (b[0], par) in bone_node:
                        L = S @ np.linalg.inv(ctx.bones[par][1])
                    err = tracks.setdefault(ni, Track(n)).put(f, L)
                    self.stats["erro_trs"] = max(self.stats["erro_trs"], err)
                    for dup in dup_bones.values():
                        if b in dup:
                            tracks.setdefault(dup[b], Track(n)).put(f, L)
            for ni, tr in tracks.items():
                tr.fill(hidden=ni in hidden)
            for ni, vals in uvs.items():
                first = next(v for v in vals if v is not None)
                for f in range(n):
                    if vals[f] is None:
                        vals[f] = vals[f - 1] if f > 0 else first
            return tracks, uvs

        def animation(gg, k, aid, tracks, uvs, times, node_index):
            """The animation's channels in gg, minus the keys that linear interpolation rebuilds within tolerance. A track that
            stays constant at the node's value (the main .glb's initial pose) is left out: when switching animations, three.js
            restores the original value of whatever the previous animation moved. Everything in one bufferView; rotation as
            normalized int16."""
            channels, samplers = [], []
            blob = bytearray()
            mine = []          # this animation's accessors (they get the bufferView at the end)
            inputs = {}        # kept keys -> times accessor (shared)

            def acc(data, ctype, count, typ, **extra):
                while len(blob) % 4:
                    blob.append(0)
                gg.j["accessors"].append(dict({"byteOffset": len(blob), "componentType": ctype, "count": count,
                                               "type": typ}, **extra))
                blob.extend(data)
                mine.append(len(gg.j["accessors"]) - 1)
                return mine[-1]

            for idx, (ni, tr) in enumerate(sorted(tracks.items())):
                nd = g.j["nodes"][ni]
                for path, vals, comps in (("translation", tr.t, 3), ("rotation", tr.q, 4), ("scale", tr.s, 3)):
                    keep = reduce_keys(vals, TOL[path])
                    self.stats["chaves_amostradas"] += len(vals)
                    # the first track always goes in, so the clip keeps its duration
                    if len(keep) == 2 and idx > 0 and const(vals, TOL[path]) and path in nd and (
                            const([vals[0], nd[path]], TOL[path]) or
                            (path == "rotation" and const([vals[0], [-c for c in nd[path]]], TOL[path]))):
                        continue
                    self.stats["chaves"] += len(keep)
                    key = tuple(keep)
                    if key not in inputs:
                        ts = [times[i] for i in keep]
                        inputs[key] = acc(struct.pack("<%df" % len(ts), *ts), 5126, len(ts), "SCALAR",
                                          min=[ts[0]], max=[ts[-1]])
                    out_vals = [vals[i] for i in keep]
                    flat = [x for v in out_vals for x in v]
                    if path == "rotation":
                        q16 = [max(-32767, min(32767, round(x * 32767))) for x in flat]
                        outp = acc(struct.pack("<%dh" % len(q16), *q16), 5122, len(out_vals), "VEC4", normalized=True)
                    else:
                        outp = acc(struct.pack("<%df" % len(flat), *flat), 5126, len(out_vals), "VEC%d" % comps)
                    samplers.append({"input": inputs[key], "output": outp, "interpolation": "LINEAR"})
                    channels.append({"sampler": len(samplers) - 1, "target": {"node": node_index(ni), "path": path}})
            bv = gg.view(bytes(blob))
            for a in mine:
                gg.j["accessors"][a]["bufferView"] = bv
            extras = {"wotmAnim": aid, "k": k, "frames": len(times)}
            if uvs:
                # scrolling texture: glTF doesn't animate UVs (three.js doesn't support KHR_animation_pointer yet); the viewer
                # reads this and shifts the mesh's texture (offset), interpolating linearly like the other tracks
                extras["uv"] = []
                for ni, vals in sorted(uvs.items()):
                    keep = reduce_keys(vals, 1e-4)
                    extras["uv"].append({"node": g.j["nodes"][ni]["name"], "t": [round(times[i], 5) for i in keep],
                                         "uv": [[round(c, 5) for c in vals[i]] for i in keep]})
            return {"name": "idle" if aid == pose.IDLE else "anim%d_k%d" % (aid, k), "channels": channels,
                    "samplers": samplers, "extras": extras}

        base = os.path.join(os.path.dirname(out), anim_dir) if anim_dir else os.path.splitext(out)[0]
        listing = []
        anims = []
        for ai, (k, aid, frames, times) in enumerate(sampled):
            tracks, uvs = tracks_of(frames)
            if ai == 0:   # the nodes' initial value = frame 0 of the first animation
                for ni, tr in tracks.items():
                    nd = g.j["nodes"][ni]
                    nd["translation"], nd["rotation"], nd["scale"] = tr.t[0], tr.q[0], tr.s[0]
            if not split or ai == 0:
                anims.append(animation(g, k, aid, tracks, uvs, times, lambda ni: ni))
            if split:
                # a .glb with only the nodes (name only, no mesh or hierarchy) and the animation: the viewer plays the clip
                # on the main model, which finds each node by name
                ga = gltf.Glb()
                ga.j["asset"]["generator"] = "WotM tools/monglb.py"
                used = sorted(tracks)
                at = {ni: i for i, ni in enumerate(used)}
                ga.j["nodes"] = [{"name": g.j["nodes"][ni]["name"]} for ni in used]
                del ga.j["meshes"]   # an empty one is a validator error
                ga.j["scenes"][0]["nodes"] = list(range(len(used)))
                ga.j["animations"] = [animation(ga, k, aid, tracks, uvs, times, at.get)]
                fn = "a%03d.glb" % k
                ga.write(os.path.join(base, fn))
                entry = {"k": k, "id": aid, "frames": len(times), "file": os.path.basename(base) + "/" + fn}
                if self.groups:
                    entry["part"] = self.groups[k // 100][0]
                if ai == 0:
                    entry["first"] = True   # the one the main .glb already carries (the viewer starts on it)
                listing.append(entry)
        if anims:
            g.j["animations"] = anims
        g.j["scenes"][0]["extras"] = {"bones": len(bone_node), "parts": len(rigid_node)}
        if uv_layer is not None:
            # Scrolling-texture layer (UV_LAYER, Kineticlops and Agamo): only the curves of the UV channels of OBJECTs with
            # uvScroll, looped on top of any animation (like native/wotm_model.hpp); it goes in here whole
            _aid, lframes, ltimes = self.sample(uv_layer, fps)
            _tr, luvs = tracks_of(lframes)
            g.j["scenes"][0]["extras"]["uvLayer"] = {"duration": ltimes[-1], "tracks": [
                {"node": g.j["nodes"][ni]["name"], "t": [round(ltimes[i], 5) for i in keep],
                 "uv": [[round(c, 5) for c in vals[i]] for i in keep]}
                for ni, vals in sorted(luvs.items()) for keep in [reduce_keys(vals, 1e-4)]]}
        if split:
            g.j["scenes"][0]["extras"]["animations"] = sorted(listing, key=lambda a: a["k"])
        g.write(out)
        st = self.stats
        print("wrote %s: %d objects, %d triangles, %d bones, %d rigid parts, %d textures (%d not found), %d animations%s; "
              "%d negative weights zeroed; max TRS error %.2g; keys %d of %d" % (
                  out, st["objetos"], st["triangulos"], len(bone_node), len(rigid_node), st["texturas"], st["sem_textura"],
                  len(sampled), " (in %s/)" % os.path.basename(base) if split else "", st["pesos_negativos"], st["erro_trs"],
                  st["chaves"], st["chaves_amostradas"]))
        return st


# Tolerance when dropping keys: linear interpolation between the remaining keys rebuilds every sampled frame with an error
# below this (translation in game units; rotation per quaternion component, ~0.06 degrees; scale relative).
TOL = {"translation": 0.01, "rotation": 5e-4, "scale": 1e-3}


UV_LAYER = 0x91   # animation that only has the scrolling-texture UV channels (the native layer's kUvLayer)


def reduce_keys(vals, tol):
    """Indexes of the keys to keep. Constant track: only the first and the last (so the clip keeps its duration)."""
    v = np.asarray(vals, dtype=np.float64)
    n = len(v)
    if n <= 2:
        return list(range(n))
    if np.abs(v - v[0]).max() <= tol:
        return [0, n - 1]
    keep, i = [0], 0
    while i < n - 1:
        j = i + 1
        while j + 1 < n:
            jj = j + 1
            t = (np.arange(i + 1, jj) - i) / (jj - i)
            lerp = v[i] + t[:, None] * (v[jj] - v[i])
            if np.abs(lerp - v[i + 1:jj]).max() > tol:
                break
            j = jj
        keep.append(j)
        i = j
    return keep


def find_nodes(img, id1):
    """{id2: node} of the stage nodes with object id id1 (top 14 bits of the first word; id2 in bits 7..17)."""
    out = {}
    d = img.d
    for o in range(0, len(d) - 4, 4):
        w = struct.unpack_from("<I", d, o)[0]
        if w >> 18 == id1:
            out.setdefault((w >> 7) & 0x7FF, []).append((img.base + o, w & 63))
    return out


def ep(img, id1, id2=0):
    """The entry node (ep) the game registers in dbProcInteractive: the GROUP/ROTATE with that id that contains the instance."""
    ci = ci_of(img, id1)
    kids = pose.Model._kids
    probe = pose.Model.__new__(pose.Model)
    probe.img = img
    nodes = [n for n, op in find_nodes(img, id1).get(id2, []) if op in (1, 3, 4)
             and any(img.u32(arr + 4 * q) == ci for arr, cnt in kids(probe, n, op) for q in range(min(cnt, 64)))]
    if not nodes:
        raise ValueError("no node %x/%d" % (id1, id2))
    return nodes[0]


def ci_of(img, id1):
    for n, op in find_nodes(img, id1).get(0, []):
        if op == 25 and img.ok(img.u32(n + 0x20)):
            return n
    raise ValueError("no CHAR_INSTANCE %x" % id1)


def place(x=0.0, y=0.0, z=0.0, heading=0.0):
    """Matrix of a _cs: RPH rotation about Z only (heading) plus translation."""
    m = np.eye(4)
    c, s_ = math.cos(heading), math.sin(heading)
    m[0, :2] = (c, -s_)
    m[1, :2] = (s_, c)
    m[3, :3] = (x, y, z)
    return m


def boss(kind, ngp_path):
    """Bosses assembled from the stage's pieces, placed the way the game places them (src/game/PlantBoss.cpp, FinalBoss.cpp):
    plant (LVL/3-MILE.NGP): the base (0x2728) at the origin and the three heads (0x271B..0x271D) where wakeUp puts them,
      each with its SWITCH 999 on; animations per piece (heads, base). root: a single root (0x271E) on its own, with its 5
      animations (the game places 10 of them around the player). mecha (LVL/CAPITOL.NGP): the robot from phase 1 of the
      final boss (0x27A6, with the turret 0x27A7 playing animation 1, which FinalBoss::initAfter turns on)."""
    img = ngp.Image.load(ngp_path)
    if kind == "plant":
        heads = [0x271B, 0x271C, 0x271D]
        pos = [(0.0, 33.884), (-29.344, -16.942), (29.344, -16.942)]   # PlantBossHead::wakeUp
        parts = [(ep(img, 0x2728), place())] + [(ep(img, h), place(x, y)) for h, (x, y) in zip(heads, pos)]
        sw = [n for h in heads for n, op in find_nodes(img, h).get(999, []) if op == 6]
        mdl = pose.Model(ngp_path, parts=parts, main=ci_of(img, 0x2728), switch_on=sw)
        groups = [("Head", [ci_of(img, h) for h in heads], 0), ("Base", [ci_of(img, 0x2728)], 1)]
        return mdl, groups, (0, 0)
    if kind == "root":
        ci = ci_of(img, 0x271E)
        mdl = pose.Model(ngp_path, parts=[(ep(img, 0x271E), place())], main=ci)
        return mdl, [("Root", [ci], 1)], (0, 1)
    if kind == "mecha":
        ci = ci_of(img, 0x27A6)
        mdl = pose.Model(ngp_path, parts=[(ep(img, 0x27A6), place())], main=ci)
        return mdl, [("", [ci], 0), ("turret", [ci_of(img, 0x27A7)], 1)], (0, 0)
    raise ValueError("unknown boss: %s" % kind)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    if "--boss" in argv:
        # python tools/monglb.py disc/LVL/3-MILE.NGP out/PLANT0.glb --boss plant --split --anim-dir PLANT
        mdl, groups, first = boss(argv[argv.index("--boss") + 1], argv[0])
        ex = Exporter(argv[0], unlit="--unlit" in argv, model=mdl, groups=groups)
        gi0, id0 = first   # (group, animation id) the model starts in
        keys = [100 * gi0 + next(a[1] for a in mdl.anims(groups[gi0][1][0]) if a[0] == id0)]
        for gi, (name, cis, _d) in enumerate(groups):
            if name.islower():   # pieces that only follow along (the robot's turret) stay out of the list
                continue
            keys += [100 * gi + a[1] for a in mdl.anims(cis[0]) if 100 * gi + a[1] != keys[0]]
        anim_dir = argv[argv.index("--anim-dir") + 1] if "--anim-dir" in argv else None
        ex.export(argv[1], keys, 60.0, split="--split" in argv, anim_dir=anim_dir)
        return 0
    ex = Exporter(argv[0], unlit="--unlit" in argv)
    table = ex.mdl.anims()
    idle_k = next(a[1] for a in table if a[0] == pose.IDLE)
    keys, fps = [idle_k], 60.0
    if "--anims" in argv:
        ids = [int(x) for x in argv[argv.index("--anims") + 1].split(",")]
        keys = [next(a[1] for a in table if a[0] == i) for i in ids]
    layer = next((a[1] for a in table if a[0] == UV_LAYER), None)
    if "--all" in argv:   # the scrolling-texture layer isn't included as an animation (it only moves the UV)
        keys = [idle_k] + [a[1] for a in table if a[1] != idle_k and a[1] != layer]
    if "--fps" in argv:
        fps = float(argv[argv.index("--fps") + 1])
    anim_dir = argv[argv.index("--anim-dir") + 1] if "--anim-dir" in argv else None
    ex.export(argv[1], keys, fps, split="--split" in argv, anim_dir=anim_dir, uv_layer=layer)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
