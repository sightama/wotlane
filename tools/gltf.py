#!/usr/bin/env python3
"""Exports the geometry of a WotM .NGP to binary glTF (.glb), with the textures from the .TEX/.RTX of the same name.

  python tools/gltf.py disc/LVL/CENTRAL.NGP out/central.glb            # finds CENTRAL.TEX and CENTRAL.RTX next to it
  python tools/gltf.py disc/LVL/CENTRAL.NGP out/central.glb --lod 0    # coarsest LOD (default: the most detailed)
  python tools/gltf.py disc/MON/CONGAR1.NGP out/congar.glb --skinned   # includes the raw skinned meshes (see below)
  options: --tex X.TEX --rtx X.RTX (other texture files), --no-tex (vertex color only), --lit (no KHR_materials_unlit)

What comes out right: static objects (stage, buildings, intact destructibles, menu), with the TRANSLATE/ROTATE/SCALE hierarchy
applied, each section's texture (Vtx.slot -> the object's texId/gsTexCtx), UV, vertex color and normals. The mesh comes from
the same decoder as the native layer (ngp.decode_mesh). Axes: the game is Z-up; glTF is Y-up (x, z, -y).

Monsters (checked on 2026-10-20 with all 13 monsters, costume 0, and the official glTF validator: 0 errors): with --skinned the
whole body comes out in the bind pose (T-pose), because the skinned mesh positions are in rest-pose model space (the game's
palette is restPoseInv * bone). Not exported yet: animation and skeleton (AnimCurve.cpp curves, SKEL_BONE/ANIM_XFORM). Rigid
parts under ANIM_XFORM come out with the identity matrix, and objects that aren't part of the body (HUD portraits, projectiles,
effects) come along too, piled up near the origin.
Sprites (the power-up halo) are also left out. Vertex color already carries the stages' baked lighting, which is why the
material is "unlit" by default.

Writes nothing from the game into the repo: it reads the user's disc and writes wherever the user says.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ngp  # noqa: E402
import tex  # noqa: E402


def find_sibling(path, ext):
    return tex.sibling(path, ext)


class Glb:
    """Binary glTF 2.0 builder: a single buffer, accessors with min/max on positions."""

    def __init__(self):
        self.bin = bytearray()
        self.j = {"asset": {"version": "2.0", "generator": "WotM tools/gltf.py"}, "scene": 0, "scenes": [{"nodes": []}],
                  "nodes": [], "meshes": [], "materials": [], "accessors": [], "bufferViews": [], "buffers": [],
                  "images": [], "textures": [], "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]}

    def view(self, data, target=None):
        while len(self.bin) % 4:
            self.bin.append(0)
        v = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data)}
        if target:
            v["target"] = target
        self.bin += data
        self.j["bufferViews"].append(v)
        return len(self.j["bufferViews"]) - 1

    def accessor(self, fmt, comps, values, ctype=5126, target=34962, minmax=False, normalized=False):
        flat = [x for v in values for x in (v if comps > 1 else (v,))]
        data = struct.pack("<%d%s" % (len(flat), fmt), *flat)
        a = {"bufferView": self.view(data, target), "componentType": ctype, "count": len(values),
             "type": {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4"}[comps]}
        if normalized:
            a["normalized"] = True
        if minmax:
            a["min"] = [min(v[k] for v in values) for k in range(comps)]
            a["max"] = [max(v[k] for v in values) for k in range(comps)]
        self.j["accessors"].append(a)
        return len(self.j["accessors"]) - 1

    def image(self, png, name):
        bv = self.view(png)
        self.j["images"].append({"bufferView": bv, "mimeType": "image/png", "name": name})
        self.j["textures"].append({"sampler": 0, "source": len(self.j["images"]) - 1})
        return len(self.j["textures"]) - 1

    def write(self, path):
        for k in ("images", "textures", "materials"):
            if not self.j[k]:
                del self.j[k]
        if "textures" not in self.j:
            del self.j["samplers"]
        while len(self.bin) % 4:
            self.bin.append(0)
        self.j["buffers"] = [{"byteLength": len(self.bin)}]
        js = json.dumps(self.j, separators=(",", ":")).encode()
        js += b" " * ((-len(js)) % 4)
        out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(self.bin))
        out += struct.pack("<II", len(js), 0x4E4F534A) + js + struct.pack("<II", len(self.bin), 0x004E4942) + bytes(self.bin)
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "wb") as f:
            f.write(out)


def to_y_up(p):
    return (p[0], p[2], -p[1])


class Materials:
    """One material per (section slot's texture, TEX0, blend), with the tex.Library image embedded in the .glb."""

    def __init__(self, g, img, lib, unlit, stats):
        self.g, self.img, self.lib, self.unlit, self.stats = g, img, lib, unlit, stats
        self.cache = {}

    def get(self, node, slot, blend, gs=None):
        """blend: the mesh's PRIM.ABE. gs: inherited GS state (pose.Item.gs: ALPHA_1, aref, zmsk, ztst) or None (the old
        default: alpha cutoff at ~6%). With gs, the material carries in extras.gs the blend mode (with ABE), the alpha test
        threshold and ZMSK; the viewer rebuilds the GS blending from them (glTF only has OPAQUE/MASK/BLEND)."""
        img, lib, g, stats = self.img, self.lib, self.g, self.stats
        gk = None
        if gs is not None:
            gk = (gs[0] if blend else None, gs[1], bool(gs[2]) if blend else False)
        if slot == ngp.SLOT_NOTEX or lib is None:
            key = ("cor", blend, gk)
        else:
            s = slot if slot < min(img.u16(node + 0x5A), 5) else 0
            tid = img.u16(node + 0x50 + 2 * s)
            t0 = img.u32(node + 0x70 + 0x30 * s) | (img.u32(node + 0x74 + 0x30 * s) << 32)
            key = ("tex", tid, t0, blend, gk)
        if key in self.cache:
            return self.cache[key]
        m = {"pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 1.0}, "doubleSided": True}
        if self.unlit:
            m["extensions"] = {"KHR_materials_unlit": {}}
        if blend:
            m["alphaMode"] = "BLEND"
        if gk is not None:
            m["extras"] = {"gs": {"blend": gk[0], "aref": gk[1], "zmsk": gk[2]}}
        if key[0] == "tex":
            im = lib.image(key[1], key[2])
            if im is None:
                stats["sem_textura"] += 1
                m["name"] = "tex%d_nao_achada" % key[1]
            else:
                w, h, px = im
                m["name"] = "tex%d" % key[1]
                m["pbrMetallicRoughness"]["baseColorTexture"] = {"index": g.image(tex.png_bytes(w, h, px), m["name"])}
                stats["texturas"] += 1
                if gk is not None:
                    # like the native shader: discard only with the alpha test on (As*128 <= aref; As = 1 at 0x80)
                    if not blend and gk[1] >= 0:
                        m["alphaMode"] = "MASK"
                        m["alphaCutoff"] = (gk[1] + 0.5) / 128.0
                elif not blend and any(a < 255 for (_r, _g, _b, a) in px):
                    m["alphaMode"] = "MASK"
                    m["alphaCutoff"] = 0.06   # the native shader discards below ~6%
        else:
            m["name"] = "cor_de_vertice"
        g.j["materials"].append(m)
        self.cache[key] = len(g.j["materials"]) - 1
        return self.cache[key]


def export(ngp_path, out, lod=99, skinned=False, tex_path=None, rtx_path=None, use_tex=True, unlit=True, quiet=False,
           items=None, model=None):
    """items/model: a tools/pose.py pose (list of OBJECTs with matrix, plus the bone palette); without them, the raw hierarchy."""
    if model is not None:
        global pose_mod
        import pose as pose_mod
    data = ngp.inflate_ngp(ngp_path)
    img = ngp.Image(data, 0)
    img.base = tex.guess_base(img)
    lib = None
    if use_tex:
        tex_path = tex_path or find_sibling(ngp_path, ".TEX")
        rtx_path = rtx_path or find_sibling(ngp_path, ".RTX")
        if tex_path or rtx_path:
            lib = tex.Library(tex_path, rtx_path, ngp_path)
    g = Glb()
    stats = {"objetos": 0, "triangulos": 0, "skinned_fora": 0, "sprites_fora": 0, "sem_textura": 0, "texturas": 0}
    if unlit:
        g.j["extensionsUsed"] = ["KHR_materials_unlit"]
    material = Materials(g, img, lib, unlit, stats).get

    seen_obj = set()

    def visit(node, mtx, ctx=None):
        mtx = [list(map(float, r)) for r in mtx]
        key = (node, tuple(map(tuple, mtx)))
        if key in seen_obj:
            return
        seen_obj.add(key)
        md = ngp.decode_mesh(img, node)
        stats["sprites_fora"] += len(md.sprites)
        if md.skinned and not skinned:
            stats["skinned_fora"] += 1
            return
        if not md.tris:
            return
        groups = {}
        for k in range(0, len(md.tris), 3):
            groups.setdefault(md.tris[k + 2].slot, []).append(md.tris[k:k + 3])
        prims = []
        rot = [r[:3] for r in mtx[:3]]
        pal = model.palette_of(node, ctx) if (model is not None and md.skinned) else None
        for slot, tris in sorted(groups.items()):
            verts, index, idx = {}, [], []
            for tri in tris:
                for v in tri:
                    sp = tuple(pose_mod.skin(pal, v)) if pal else (v.x, v.y, v.z)
                    p = to_y_up(ngp.xform(mtx, sp))
                    n = (v.nx * rot[0][0] + v.ny * rot[1][0] + v.nz * rot[2][0], v.nx * rot[0][1] + v.ny * rot[1][1] + v.nz * rot[2][1],
                         v.nx * rot[0][2] + v.ny * rot[1][2] + v.nz * rot[2][2])
                    ln = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) ** 0.5
                    n = to_y_up((n[0] / ln, n[1] / ln, n[2] / ln)) if ln > 1e-6 else (0.0, 1.0, 0.0)
                    c = (min(1.0, v.r / 128.0), min(1.0, v.g / 128.0), min(1.0, v.b / 128.0), min(1.0, v.a / 128.0) if md.abe else 1.0)
                    k = (p, n, (v.u, v.v), c)
                    if k not in verts:
                        verts[k] = len(verts)
                        index.append(k)
                    idx.append(verts[k])
            attrs = {"POSITION": g.accessor("f", 3, [k[0] for k in index], minmax=True),
                     "NORMAL": g.accessor("f", 3, [k[1] for k in index]),
                     "COLOR_0": g.accessor("f", 4, [k[3] for k in index])}
            if slot != ngp.SLOT_NOTEX and lib is not None:
                attrs["TEXCOORD_0"] = g.accessor("f", 2, [k[2] for k in index])
            ctype, fmt = (5123, "H") if len(index) < 65536 else (5125, "I")
            prims.append({"attributes": attrs, "indices": g.accessor(fmt, 1, idx, ctype=ctype, target=34963),
                          "material": material(node, slot, md.abe)})
            stats["triangulos"] += len(tris)
        g.j["meshes"].append({"name": "obj_%x" % node, "primitives": prims})
        g.j["nodes"].append({"name": "obj_%x" % node, "mesh": len(g.j["meshes"]) - 1})
        g.j["scenes"][0]["nodes"].append(len(g.j["nodes"]) - 1)
        stats["objetos"] += 1

    if items is not None:   # pose built by tools/pose.py: (OBJECT, matrix, instance)
        for node, mtx, ctx in items:
            visit(node, mtx, ctx)
    else:
        for r in img.roots():
            ngp.walk(img, r, visit, lod=lod)
    g.write(out)
    if not quiet:
        print("wrote %s: %d objects, %d triangles, %d textures (%d not found); skipped: %d skinned, %d sprites" % (
            out, stats["objetos"], stats["triangulos"], stats["texturas"], stats["sem_textura"], stats["skinned_fora"],
            stats["sprites_fora"]))
    return stats


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    args, opts, i = [], {}, 0
    while i < len(argv):
        a = argv[i]
        if a in ("--lod", "--tex", "--rtx"):
            opts[a[2:]] = argv[i + 1]
            i += 2
            continue
        if a in ("--skinned", "--no-tex", "--lit"):
            opts[a[2:]] = True
        else:
            args.append(a)
        i += 1
    export(args[0], args[1], lod=int(opts.get("lod", 99)), skinned=opts.get("skinned", False), tex_path=opts.get("tex"),
           rtx_path=opts.get("rtx"), use_tex=not opts.get("no-tex", False), unlit=not opts.get("lit", False))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
