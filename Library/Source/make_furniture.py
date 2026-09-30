# The furniture of 101st Floor's built-in library (ADR-0019), modelled in code with Blender so it can be regenerated from
# Git. Run through Scripts/GenerateLibrary.ps1, or:
#   & "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python Library/Source/make_furniture.py
# Writes Library/furniture/<id>.glb (metres, glTF +Y up, the front towards glTF +Z: Blender -Y), <id>.png (a picture for
# the palette) and manifest.json. Pieces that go against a wall have their back towards Blender +Y.
import json
import math
import os
import sys

import bpy
import mathutils
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "furniture"))
CREDIT = "101st Floor (modelado con Blender)"
THUMBNAIL = 256

# ---------------------------------------------------------------------------------------------------------------- images


def image(name, pixels):
    # pixels: (h, w, 3) linear colour, rows from the bottom.
    h, w = pixels.shape[:2]
    img = bpy.data.images.new(name, width=w, height=h, alpha=False)
    rgba = np.concatenate([pixels, np.ones((h, w, 1))], axis=2)
    img.pixels.foreach_set(rgba.astype(np.float32).ravel())
    img.pack()
    return img


def wood_grain(tone, size=256):
    y, x = np.mgrid[0:size, 0:size] / size
    rings = np.sin((y * 18 + 1.5 * np.sin(x * 6.3 + y * 3.1)) * math.pi)
    fibre = 0.04 * np.sin(y * 420 + np.sin(x * 40) * 3)
    shade = 0.88 + 0.08 * rings + fibre
    return np.clip(np.array(tone)[None, None, :] * shade[..., None], 0, 1)


def weave(tone, size=128):
    y, x = np.mgrid[0:size, 0:size]
    warp = ((x // 2 + y // 2) % 2) * 0.06
    noise = np.sin(x * 12.9898 + y * 78.233) * 43758.5453
    noise = (noise - np.floor(noise)) * 0.05
    shade = 0.92 + warp + noise
    return np.clip(np.array(tone)[None, None, :] * shade[..., None], 0, 1)


def rug(colours, size=256, round_rug=False):
    y, x = np.mgrid[0:size, 0:size] / size
    u, v = x - 0.5, y - 0.5
    if round_rug:
        r = np.sqrt(u * u + v * v) * 2
        band = np.floor(r * 6) % len(colours)
    else:
        border = np.minimum(np.minimum(x, 1 - x), np.minimum(y, 1 - y))
        diamond = (np.abs(u) + np.abs(v)) * 8
        band = np.where(border < 0.08, 0, np.where(border < 0.11, 1, (np.floor(diamond) % 2) + 1))
    band = band.astype(int) % len(colours)
    palette = np.array(colours)
    noise = np.sin(x * 900) * np.sin(y * 870) * 0.03
    return np.clip(palette[band] * (1 + noise[..., None]), 0, 1)


# ------------------------------------------------------------------------------------------------------------- materials

MATERIALS = {}


def srgb(c):
    # sRGB 0..255 to linear.
    def one(v):
        v = v / 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return tuple(one(v) for v in c)


def material(name, colour=(200, 200, 200), roughness=0.6, metallic=0.0, texture=None):
    if name in MATERIALS:
        return MATERIALS[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*srgb(colour), 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if texture is not None:
        node = mat.node_tree.nodes.new("ShaderNodeTexImage")
        node.image = texture
        mat.node_tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
    MATERIALS[name] = mat
    return mat


def palette():
    # Shared looks, made once per piece (the scene is cleared between pieces).
    MATERIALS.clear()
    return {
        "oak": material("Oak", texture=image("oak", wood_grain(srgb((196, 150, 102)))), roughness=0.55),
        "walnut": material("Walnut", texture=image("walnut", wood_grain(srgb((110, 72, 48)))), roughness=0.5),
        "ash": material("Ash", texture=image("ash", wood_grain(srgb((222, 200, 168)))), roughness=0.55),
        "black-wood": material("Black wood", (40, 38, 36), 0.45),
        "white": material("White lacquer", (236, 234, 228), 0.35),
        "grey": material("Grey lacquer", (138, 140, 142), 0.4),
        "sage": material("Sage lacquer", (150, 170, 146), 0.4),
        "navy": material("Navy lacquer", (44, 58, 88), 0.4),
        "steel": material("Brushed steel", (190, 192, 194), 0.3, 1.0),
        "black-metal": material("Black metal", (30, 30, 32), 0.35, 1.0),
        "brass": material("Brass", (214, 170, 96), 0.25, 1.0),
        "chrome": material("Chrome", (230, 230, 232), 0.08, 1.0),
        "linen": material("Linen", texture=image("linen", weave(srgb((226, 218, 202)))), roughness=0.9),
        "grey-fabric": material("Grey fabric", texture=image("grey-fabric", weave(srgb((128, 130, 134)))), roughness=0.95),
        "blue-fabric": material("Blue fabric", texture=image("blue-fabric", weave(srgb((70, 92, 132)))), roughness=0.95),
        "green-velvet": material("Green velvet", texture=image("green-velvet", weave(srgb((58, 98, 76)))), roughness=0.8),
        "mustard": material("Mustard fabric", texture=image("mustard", weave(srgb((206, 158, 60)))), roughness=0.9),
        "terracotta-fabric": material("Terracotta fabric", texture=image("terracotta", weave(srgb((178, 102, 74)))), roughness=0.9),
        "cream": material("Cream fabric", texture=image("cream", weave(srgb((236, 228, 210)))), roughness=0.9),
        "leather": material("Cognac leather", (140, 82, 46), 0.45),
        "black-leather": material("Black leather", (34, 32, 32), 0.4),
        "ceramic": material("Ceramic", (245, 245, 242), 0.1),
        "marble": material("Marble", (232, 230, 226), 0.15),
        "glass-dark": material("Dark glass", (30, 32, 34), 0.05),
        "mirror": material("Mirror", (210, 216, 220), 0.02, 1.0),
        "leaf": material("Leaf", (58, 110, 52), 0.6),
        "leaf-dark": material("Dark leaf", (36, 78, 40), 0.6),
        "soil": material("Soil", (60, 44, 32), 0.95),
        "terracotta": material("Terracotta", (180, 100, 66), 0.8),
        "concrete": material("Concrete", (160, 158, 152), 0.85),
        "mattress": material("Mattress", (240, 238, 232), 0.9),
        "shade": material("Lamp shade", (244, 236, 216), 0.9),
        "bulb": material("Frosted glass", (250, 248, 240), 0.2),
        "ink": material("Black", (20, 20, 22), 0.5),
    }


# ----------------------------------------------------------------------------------------------------------- primitives


def finish(obj, mat, smooth=False, bevel=0.0):
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    if bevel > 0.0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.limit_method = "ANGLE"
    if smooth:
        bpy.ops.object.shade_smooth()
    else:
        bpy.ops.object.shade_flat()
    return obj


def box(size, loc, mat, bevel=0.004, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.scale = size
    bpy.ops.object.transform_apply(scale=True)
    return finish(obj, mat, bevel=bevel)


def cyl(radius, depth, loc, mat, verts=32, rot=(0, 0, 0), bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.active_object
    return finish(obj, mat, smooth=True, bevel=bevel)


def cone(r1, r2, depth, loc, mat, verts=32, rot=(0, 0, 0), cap=True):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r1, radius2=r2, depth=depth, location=loc, rotation=rot,
                                    end_fill_type="NGON" if cap else "NOTHING")
    obj = bpy.context.active_object
    return finish(obj, mat, smooth=True)


def ball(radius, loc, mat, scale=(1, 1, 1), segments=24):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=segments // 2, radius=radius, location=loc)
    obj = bpy.context.active_object
    obj.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    return finish(obj, mat, smooth=True)


def ring(major, minor, loc, mat, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, location=loc, rotation=rot,
                                     major_segments=32, minor_segments=8)
    obj = bpy.context.active_object
    return finish(obj, mat, smooth=True)


def rod(a, b, radius, mat, verts=12):
    # A cylinder from point a to point b.
    a, b = np.array(a, float), np.array(b, float)
    d = b - a
    length = float(np.linalg.norm(d))
    mid = (a + b) / 2
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=length, location=tuple(mid))
    obj = bpy.context.active_object
    z = np.array([0.0, 0.0, 1.0])
    axis = np.cross(z, d / length)
    angle = math.acos(max(-1.0, min(1.0, float(np.dot(z, d / length)))))
    if np.linalg.norm(axis) > 1e-9:
        obj.rotation_mode = "AXIS_ANGLE"
        obj.rotation_axis_angle = (angle, *(axis / np.linalg.norm(axis)))
    return finish(obj, mat, smooth=True)


def legs(width, depth, height, mat, inset=0.04, radius=0.02, taper=None):
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (width / 2 - inset), sy * (depth / 2 - inset)
            if taper:
                cone(taper, radius, height, (x, y, height / 2), mat, verts=16)  # thinner at the foot
            else:
                cyl(radius, height, (x, y, height / 2), mat, verts=16)


# --------------------------------------------------------------------------------------------------------------- pieces
# Each builder models one piece around the origin, on z = 0, its front towards -Y.


def chair_classic(m):
    legs(0.44, 0.46, 0.45, m["oak"], radius=0.018)
    box((0.46, 0.46, 0.04), (0, 0, 0.47), m["oak"])
    for sx in (-1, 1):
        box((0.035, 0.035, 0.5), (sx * 0.2, 0.21, 0.74), m["oak"])
    for i in range(3):
        box((0.035, 0.02, 0.36), (-0.1 + i * 0.1, 0.215, 0.7), m["oak"])
    box((0.44, 0.03, 0.07), (0, 0.215, 0.95), m["oak"])


def chair_windsor(m):
    cyl(0.23, 0.045, (0, 0, 0.45), m["walnut"], verts=40)
    for angle in (45, 135, 225, 315):
        a = math.radians(angle)
        rod((0.16 * math.cos(a), 0.16 * math.sin(a), 0.44), (0.22 * math.cos(a), 0.22 * math.sin(a), 0.0), 0.017, m["walnut"])
    for i in range(7):
        x = -0.18 + i * 0.06
        rod((x, 0.15, 0.47), (x * 1.08, 0.19, 0.9), 0.009, m["walnut"])
    box((0.46, 0.045, 0.06), (0, 0.19, 0.92), m["walnut"], bevel=0.012)


def chair_upholstered(m):
    for sx in (-1, 1):
        for sy in (-1, 1):
            rod((sx * 0.19, sy * 0.19, 0.0), (sx * 0.2, sy * 0.2, 0.44), 0.012, m["black-metal"])
    box((0.46, 0.46, 0.09), (0, 0, 0.48), m["green-velvet"], bevel=0.02)
    box((0.44, 0.08, 0.42), (0, 0.2, 0.78), m["green-velvet"], bevel=0.025, rot=(math.radians(-8), 0, 0))


def chair_scandi(m):
    legs(0.42, 0.42, 0.44, m["ash"], radius=0.02, taper=0.012)
    box((0.46, 0.44, 0.03), (0, 0, 0.455), m["white"], bevel=0.012)
    box((0.46, 0.03, 0.3), (0, 0.22, 0.66), m["white"], bevel=0.012, rot=(math.radians(-10), 0, 0))
    for sx in (-1, 1):
        rod((sx * 0.2, 0.19, 0.46), (sx * 0.21, 0.21, 0.56), 0.012, m["ash"])


def chair_office(m):
    for i in range(5):
        a = math.radians(90 + i * 72)
        rod((0, 0, 0.07), (0.3 * math.cos(a), 0.3 * math.sin(a), 0.05), 0.02, m["black-metal"])
        ball(0.025, (0.3 * math.cos(a), 0.3 * math.sin(a), 0.025), m["ink"])
    cyl(0.025, 0.35, (0, 0, 0.25), m["chrome"])
    box((0.5, 0.48, 0.08), (0, 0, 0.47), m["black-leather"], bevel=0.02)
    box((0.46, 0.06, 0.55), (0, 0.24, 0.82), m["black-leather"], bevel=0.02, rot=(math.radians(-6), 0, 0))
    for sx in (-1, 1):
        box((0.04, 0.25, 0.025), (sx * 0.27, 0.02, 0.66), m["ink"])
        box((0.025, 0.03, 0.16), (sx * 0.27, 0.05, 0.57), m["ink"])


def stool_bar(m):
    for sx in (-1, 1):
        for sy in (-1, 1):
            rod((sx * 0.13, sy * 0.13, 0.74), (sx * 0.18, sy * 0.18, 0.0), 0.014, m["black-metal"])
    ring(0.155, 0.009, (0, 0, 0.3), m["black-metal"])
    cyl(0.19, 0.05, (0, 0, 0.765), m["walnut"], verts=40)


def stool_low(m):
    for i in range(3):
        a = math.radians(90 + i * 120)
        rod((0.11 * math.cos(a), 0.11 * math.sin(a), 0.44), (0.16 * math.cos(a), 0.16 * math.sin(a), 0.0), 0.016, m["oak"])
    cyl(0.17, 0.035, (0, 0, 0.455), m["oak"], verts=40)


def bench(m):
    legs(1.2, 0.36, 0.43, m["oak"], radius=0.025, taper=0.016, inset=0.1)
    box((1.2, 0.36, 0.04), (0, 0, 0.45), m["oak"], bevel=0.008)


def sofa(m, width, fabric, seats, depth=0.92):
    arm = 0.16
    for sx in (-1, 1):
        for sy in (-1, 1):
            cone(0.02, 0.014, 0.1, (sx * (width / 2 - 0.07), sy * (depth / 2 - 0.07), 0.05), m["walnut"], verts=12)
    box((width, depth, 0.24), (0, 0, 0.22), fabric, bevel=0.03)
    for sx in (-1, 1):
        box((arm, depth, 0.36), (sx * (width / 2 - arm / 2), 0, 0.46), fabric, bevel=0.05)
    box((width - 2 * arm, 0.2, 0.44), (0, depth / 2 - 0.1, 0.56), fabric, bevel=0.05)
    inner = width - 2 * arm
    for i in range(seats):
        w = inner / seats
        x = -inner / 2 + w * (i + 0.5)
        box((w - 0.01, depth - 0.22, 0.14), (x, -0.1, 0.41), fabric, bevel=0.04)
        box((w - 0.03, 0.14, 0.36), (x, depth / 2 - 0.26, 0.62), fabric, bevel=0.05, rot=(math.radians(-10), 0, 0))


def sofa_corner(m):
    fabric = m["grey-fabric"]
    sofa(m, 2.4, fabric, 3)
    # the chaise: the left end, 1.6 m deep
    box((0.8, 0.7, 0.24), (-0.8, -0.8, 0.22), fabric, bevel=0.03)
    box((0.79, 0.7, 0.14), (-0.8, -0.8, 0.41), fabric, bevel=0.04)


def sofa_chaise(m):
    fabric = m["cream"]
    sofa(m, 2.3, fabric, 2)
    box((0.75, 0.75, 0.24), (0.7, -0.83, 0.22), fabric, bevel=0.03)
    box((0.74, 0.75, 0.14), (0.7, -0.83, 0.41), fabric, bevel=0.04)


def armchair_club(m):
    leather = m["leather"]
    box((0.9, 0.9, 0.3), (0, 0, 0.2), leather, bevel=0.04)
    for sx in (-1, 1):
        box((0.2, 0.9, 0.42), (sx * 0.35, 0, 0.5), leather, bevel=0.07)
    box((0.9, 0.22, 0.5), (0, 0.34, 0.58), leather, bevel=0.07)
    box((0.5, 0.66, 0.14), (0, -0.1, 0.42), leather, bevel=0.05)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cyl(0.025, 0.05, (sx * 0.38, sy * 0.38, 0.025), m["black-wood"], verts=12)


def armchair_lounge(m):
    for sx in (-1, 1):
        rod((sx * 0.34, -0.36, 0.0), (sx * 0.34, -0.3, 0.6), 0.02, m["walnut"])
        rod((sx * 0.34, 0.34, 0.0), (sx * 0.36, 0.42, 0.95), 0.02, m["walnut"])
        box((0.06, 0.7, 0.03), (sx * 0.34, 0.0, 0.6), m["walnut"])
    box((0.62, 0.6, 0.12), (0, -0.02, 0.4), m["mustard"], bevel=0.04)
    box((0.62, 0.12, 0.55), (0, 0.33, 0.72), m["mustard"], bevel=0.05, rot=(math.radians(-15), 0, 0))
    box((0.6, 0.04, 0.03), (0, -0.33, 0.33), m["walnut"])


def pouf(m):
    cyl(0.25, 0.42, (0, 0, 0.21), m["terracotta-fabric"], verts=40, bevel=0.03)


def table_rect(m):
    legs(1.8, 0.9, 0.72, m["oak"], radius=0.035, inset=0.08)
    box((1.8, 0.9, 0.04), (0, 0, 0.74), m["oak"], bevel=0.006)
    box((1.6, 0.03, 0.08), (0, 0.38, 0.66), m["oak"])
    box((1.6, 0.03, 0.08), (0, -0.38, 0.66), m["oak"])


def table_round(m):
    cyl(0.6, 0.035, (0, 0, 0.74), m["walnut"], verts=64)
    cyl(0.06, 0.66, (0, 0, 0.39), m["walnut"], verts=24)
    cyl(0.28, 0.04, (0, 0, 0.02), m["walnut"], verts=48)


def table_coffee(m):
    legs(1.1, 0.6, 0.38, m["black-metal"], radius=0.012, inset=0.03)
    box((1.1, 0.6, 0.03), (0, 0, 0.395), m["marble"], bevel=0.006)
    box((1.04, 0.54, 0.02), (0, 0, 0.12), m["oak"])


def table_side(m):
    cyl(0.23, 0.025, (0, 0, 0.53), m["oak"], verts=48)
    for i in range(3):
        a = math.radians(90 + i * 120)
        rod((0.12 * math.cos(a), 0.12 * math.sin(a), 0.52), (0.17 * math.cos(a), 0.17 * math.sin(a), 0.0), 0.012, m["black-metal"])


def desk(m):
    box((1.4, 0.7, 0.03), (0, 0, 0.745), m["ash"], bevel=0.005)
    box((0.42, 0.62, 0.7), (0.47, 0, 0.365), m["white"], bevel=0.004)
    for i in range(3):
        box((0.38, 0.01, 0.2), (0.47, -0.311, 0.14 + i * 0.22), m["white"])
        box((0.1, 0.012, 0.012), (0.47, -0.32, 0.2 + i * 0.22), m["steel"])
    for sy in (-1, 1):
        box((0.04, 0.04, 0.73), (-0.66, sy * 0.3, 0.365), m["white"])


def table_console(m):
    legs(1.1, 0.35, 0.76, m["black-metal"], radius=0.012, inset=0.03)
    box((1.1, 0.35, 0.03), (0, 0, 0.775), m["walnut"], bevel=0.005)


def bedside(m):
    legs(0.45, 0.38, 0.12, m["oak"], radius=0.014, inset=0.04)
    box((0.45, 0.38, 0.4), (0, 0, 0.32), m["oak"], bevel=0.005)
    box((0.41, 0.01, 0.16), (0, -0.19, 0.4), m["white"])
    box((0.08, 0.015, 0.012), (0, -0.2, 0.42), m["brass"])


def bed(m, width, length, headboard, frame, head_height=1.1, mattress_top=0.55, low_legs=True):
    if low_legs:
        legs(width, length, 0.12, frame, radius=0.025, inset=0.08)
    box((width, length, 0.25), (0, 0.0, 0.245), frame, bevel=0.01)
    box((width - 0.06, length - 0.08, 0.22), (0, -0.02, mattress_top - 0.11), m["mattress"], bevel=0.03)
    box((width + 0.06, 0.1, head_height), (0, length / 2 + 0.05, head_height / 2), headboard, bevel=0.03)
    box((width - 0.1, length * 0.55, 0.06), (0, -length * 0.2, mattress_top + 0.03), m["linen"], bevel=0.02)
    for sx in ((-1, 1) if width > 1.2 else (0,)):
        box((0.55, 0.35, 0.12), (sx * width / 4 if width > 1.2 else 0, length / 2 - 0.25, mattress_top + 0.06), m["cream"], bevel=0.05)


def bed_upholstered(m):
    bed(m, 1.6, 2.1, m["grey-fabric"], m["grey-fabric"], head_height=1.15)


def bed_king(m):
    bed(m, 1.9, 2.1, m["walnut"], m["walnut"], head_height=1.0)


def bed_iron(m):
    frame = m["black-metal"]
    box((0.92, 2.0, 0.2), (0, 0, 0.42), m["mattress"], bevel=0.03)
    box((0.86, 1.1, 0.05), (0, -0.4, 0.545), m["blue-fabric"], bevel=0.02)
    box((0.55, 0.35, 0.12), (0, 0.72, 0.58), m["cream"], bevel=0.05)
    for sy, h in ((1, 1.1), (-1, 0.85)):
        for sx in (-1, 1):
            cyl(0.02, h, (sx * 0.47, sy * 1.0, h / 2), frame, verts=12)
        rod((-0.47, sy * 1.0, h - 0.02), (0.47, sy * 1.0, h - 0.02), 0.015, frame)
        rod((-0.47, sy * 1.0, 0.35), (0.47, sy * 1.0, 0.35), 0.012, frame)
        for i in range(5):
            x = -0.3 + i * 0.15
            rod((x, sy * 1.0, 0.35), (x, sy * 1.0, h - 0.02), 0.007, frame)
    for sx in (-1, 1):
        rod((sx * 0.47, -1.0, 0.33), (sx * 0.47, 1.0, 0.33), 0.012, frame)


def bunk_bed(m):
    wood = m["ash"]
    for sx in (-1, 1):
        for sy in (-1, 1):
            box((0.06, 0.06, 1.65), (sx * 0.47, sy * 1.0, 0.825), wood)
    for z in (0.3, 1.25):
        box((1.0, 2.06, 0.12), (0, 0, z), wood)
        box((0.9, 1.96, 0.16), (0, 0, z + 0.14), m["mattress"], bevel=0.03)
        box((0.86, 1.0, 0.04), (0, -0.45, z + 0.24), m["blue-fabric"], bevel=0.015)
    for sy in (-1, 1):
        box((1.0, 0.04, 0.2), (0, sy * 1.0, 1.55), wood)
    box((0.04, 1.2, 0.2), (0.47, 0.35, 1.55), wood)
    for i in range(5):
        box((0.3, 0.03, 0.03), (-0.34, -0.98, 0.35 + i * 0.25), wood)


def crib(m):
    wood = m["white"]
    for sx in (-1, 1):
        for sy in (-1, 1):
            box((0.05, 0.05, 0.9), (sx * 0.34, sy * 0.64, 0.45), wood)
    for sx in (-1, 1):
        for z in (0.3, 0.87):
            box((0.05, 1.28, 0.04), (sx * 0.34, 0, z), wood)
        for i in range(11):
            cyl(0.01, 0.55, (sx * 0.34, -0.55 + i * 0.11, 0.585), wood, verts=8)
    for sy in (-1, 1):
        box((0.68, 0.04, 0.6), (0, sy * 0.64, 0.6), wood)
    box((0.64, 1.24, 0.1), (0, 0, 0.35), m["mattress"], bevel=0.02)


def bookshelf(m):
    wood = m["oak"]
    width, depth, height = 0.9, 0.32, 1.9
    for sx in (-1, 1):
        box((0.025, depth, height), (sx * (width / 2 - 0.0125), 0, height / 2), wood)
    box((width, 0.01, height), (0, depth / 2 - 0.005, height / 2), wood)
    for i in range(6):
        box((width - 0.05, depth - 0.01, 0.025), (0, -0.005, 0.02 + i * 0.37), wood)
    colours = [m["navy"], m["sage"], m["terracotta"], m["white"], m["mustard"], m["grey"]]
    for shelf in range(1, 5):
        x = -0.38
        k = shelf
        while x < 0.3:
            w = 0.025 + 0.015 * ((k * 7) % 3)
            h = 0.22 + 0.04 * ((k * 5) % 3)
            box((w, 0.2, h), (x + w / 2, 0.02, 0.035 + shelf * 0.37 + h / 2), colours[k % len(colours)], bevel=0.002)
            x += w + 0.004
            k += 1


def sideboard(m):
    legs(1.8, 0.45, 0.14, m["black-metal"], radius=0.014, inset=0.06)
    box((1.8, 0.45, 0.64), (0, 0, 0.46), m["walnut"], bevel=0.006)
    for i in range(4):
        x = -0.675 + i * 0.45
        box((0.43, 0.01, 0.58), (x, -0.226, 0.46), m["walnut"])
        box((0.012, 0.012, 0.12), (x + 0.18, -0.235, 0.48), m["brass"])


def dresser(m):
    legs(1.0, 0.48, 0.1, m["oak"], radius=0.018, inset=0.05)
    box((1.0, 0.48, 0.76), (0, 0, 0.48), m["white"], bevel=0.006)
    for i in range(3):
        z = 0.22 + i * 0.25
        box((0.96, 0.01, 0.23), (0, -0.241, z), m["white"])
        box((0.2, 0.015, 0.02), (0, -0.25, z + 0.05), m["oak"])


def tv_unit(m):
    box((1.8, 0.42, 0.45), (0, 0, 0.225), m["grey"], bevel=0.005)
    for i in range(3):
        box((0.58, 0.01, 0.2), (-0.6 + i * 0.6, -0.211, 0.3), m["oak"])
    box((1.2, 0.05, 0.7), (0, 0.1, 0.8), m["ink"], bevel=0.008)
    box((0.3, 0.2, 0.02), (0, 0.1, 0.46), m["ink"])


def wardrobe_sliding(m):
    box((2.0, 0.62, 2.3), (0, 0, 1.15), m["white"], bevel=0.005)
    box((1.0, 0.02, 2.24), (-0.5, -0.32, 1.15), m["oak"])
    box((1.0, 0.02, 2.24), (0.5, -0.34, 1.15), m["mirror"])
    box((0.01, 0.02, 2.24), (0.0, -0.345, 1.15), m["steel"])


def shoe_cabinet(m):
    box((0.8, 0.26, 1.05), (0, 0, 0.525), m["sage"], bevel=0.006)
    for i in range(3):
        box((0.78, 0.01, 0.33), (0, -0.131, 0.18 + i * 0.345), m["sage"])
        box((0.12, 0.012, 0.012), (0, -0.14, 0.3 + i * 0.345), m["brass"])


def kitchen_island(m):
    box((1.8, 0.9, 0.86), (0, 0.05, 0.43), m["navy"], bevel=0.005)
    box((2.0, 1.1, 0.04), (0, -0.05, 0.88), m["marble"], bevel=0.005)
    for i in range(3):
        box((0.56, 0.01, 0.7), (-0.6 + i * 0.6, 0.506, 0.43), m["navy"])
    for i in range(2):
        stool_x = -0.45 + i * 0.9
        cyl(0.17, 0.05, (stool_x, -0.75, 0.66), m["oak"], verts=32)
        cyl(0.025, 0.62, (stool_x, -0.75, 0.33), m["black-metal"])
        cyl(0.2, 0.02, (stool_x, -0.75, 0.01), m["black-metal"], verts=32)


def range_cooker(m):
    box((0.9, 0.6, 0.86), (0, 0, 0.43), m["steel"], bevel=0.004)
    box((0.9, 0.6, 0.02), (0, 0, 0.87), m["ink"])
    for sx in (-1, 1):
        for sy in (-1, 1):
            ring(0.07 if sx < 0 else 0.055, 0.008, (sx * 0.22, sy * 0.14, 0.885), m["black-metal"])
    box((0.8, 0.01, 0.45), (0, -0.301, 0.38), m["glass-dark"])
    box((0.6, 0.02, 0.02), (0, -0.32, 0.66), m["chrome"])
    for i in range(5):
        cyl(0.02, 0.03, (-0.3 + i * 0.15, -0.31, 0.78), m["ink"], rot=(math.pi / 2, 0, 0), verts=16)


def dishwasher(m):
    box((0.6, 0.58, 0.82), (0, 0, 0.41), m["steel"], bevel=0.004)
    box((0.56, 0.01, 0.1), (0, -0.291, 0.74), m["ink"])
    box((0.4, 0.02, 0.02), (0, -0.305, 0.66), m["chrome"])


def fridge_american(m):
    box((0.91, 0.72, 1.78), (0, 0, 0.89), m["steel"], bevel=0.008)
    box((0.005, 0.02, 1.7), (0, -0.37, 0.9), m["ink"])
    for sx in (-1, 1):
        box((0.02, 0.03, 0.9), (sx * 0.04, -0.39, 1.0), m["chrome"])
    box((0.18, 0.01, 0.28), (-0.22, -0.365, 1.25), m["glass-dark"])


def subtract(target, cutter):
    # target minus cutter (applied now), and the cutter removed.
    mod = target.modifiers.new("Cut", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = cutter
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = target
    target.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter, do_unlink=True)


def bathtub_freestanding(m):
    # The lower half of an ellipsoid, hollowed: a bowl 1.7 m long and 60 cm tall.
    tub = ball(0.5, (0, 0, 0.6), m["ceramic"], scale=(1.7, 0.8, 1.2), segments=48)
    subtract(tub, box((2.0, 1.0, 1.0), (0, 0, 1.1), m["ceramic"], bevel=0.0))
    subtract(tub, ball(0.5, (0, 0, 0.64), m["ceramic"], scale=(1.58, 0.66, 1.08), segments=48))
    cyl(0.018, 0.95, (0.9, 0.0, 0.475), m["chrome"], verts=16)
    rod((0.9, 0.0, 0.95), (0.76, 0.0, 0.95), 0.012, m["chrome"])


def vanity_double(m):
    box((1.4, 0.5, 0.5), (0, 0, 0.55), m["oak"], bevel=0.006)
    box((1.42, 0.52, 0.03), (0, 0, 0.815), m["marble"], bevel=0.004)
    for sx in (-1, 1):
        ball(0.2, (sx * 0.35, -0.02, 0.87), m["ceramic"], scale=(1.0, 0.85, 0.3))
        rod((sx * 0.35, 0.18, 0.83), (sx * 0.35, 0.18, 1.0), 0.012, m["chrome"])
        rod((sx * 0.35, 0.18, 1.0), (sx * 0.35, 0.08, 1.0), 0.01, m["chrome"])
    box((1.2, 0.03, 0.7), (0, 0.24, 1.45), m["mirror"], bevel=0.004)
    for sx in (-1, 1):
        cyl(0.02, 0.2, (sx * 0.62, 0.12, 0.1), m["chrome"])


def toilet_wall(m):
    box((0.38, 0.14, 0.9), (0, 0.29, 0.45), m["white"], bevel=0.01)
    ball(0.2, (0, 0.02, 0.36), m["ceramic"], scale=(0.9, 1.35, 0.55))
    box((0.36, 0.46, 0.03), (0, 0.02, 0.44), m["ceramic"], bevel=0.012)
    box((0.12, 0.02, 0.08), (0, 0.22, 0.78), m["chrome"])


def towel_radiator(m):
    for sx in (-1, 1):
        cyl(0.018, 1.2, (sx * 0.25, 0.06, 0.7), m["chrome"], verts=16)
    for i in range(12):
        rod((-0.25, 0.06, 0.18 + i * 0.09), (0.25, 0.06, 0.18 + i * 0.09), 0.009, m["chrome"])
    box((0.5, 0.02, 0.45), (0, 0.02, 0.9), m["linen"], bevel=0.004)


def lamp_arc(m):
    box((0.35, 0.25, 0.08), (0, 0.9, 0.04), m["marble"], bevel=0.01)
    points = [(0, 0.9, 0.08)] + [(0, 0.9 - 1.0 * math.sin(t), 0.08 + 1.9 * math.sin(t * 1.3))
                                  for t in np.linspace(0.1, math.pi / 2, 12)]
    for a, b in zip(points, points[1:]):
        rod(a, b, 0.012, m["steel"])
    end = points[-1]
    ball(0.2, (end[0], end[1] - 0.05, end[2] - 0.22), m["steel"], scale=(1, 1, 0.55))
    ball(0.06, (end[0], end[1] - 0.05, end[2] - 0.3), m["bulb"])


def lamp_tripod(m):
    for i in range(3):
        a = math.radians(90 + i * 120)
        rod((0.3 * math.cos(a), 0.3 * math.sin(a), 0.0), (0.02 * math.cos(a), 0.02 * math.sin(a), 1.2), 0.015, m["walnut"])
    cone(0.26, 0.2, 0.32, (0, 0, 1.38), m["shade"], cap=False)
    ball(0.05, (0, 0, 1.3), m["bulb"])


def lamp_globe(m):
    cyl(0.15, 0.03, (0, 0, 0.015), m["brass"], verts=40)
    cyl(0.012, 1.3, (0, 0, 0.68), m["brass"], verts=12)
    ball(0.17, (0, 0, 1.45), m["bulb"])


def lamp_reading(m):
    cyl(0.14, 0.025, (0, 0, 0.0125), m["black-metal"], verts=40)
    cyl(0.01, 1.2, (0, 0, 0.62), m["black-metal"], verts=12)
    rod((0, 0, 1.22), (0, -0.28, 1.34), 0.009, m["black-metal"])
    cone(0.08, 0.04, 0.16, (0, -0.32, 1.26), m["black-metal"], rot=(math.radians(35), 0, 0), cap=False)


def plant(m, pot_radius, pot_height, kind):
    cone(pot_radius * 0.78, pot_radius, pot_height, (0, 0, pot_height / 2), m["terracotta"] if kind != "cactus" else m["concrete"])
    cyl(pot_radius * 0.92, 0.02, (0, 0, pot_height - 0.02), m["soil"])
    if kind == "monstera":
        for i in range(9):
            a = math.radians(i * 40)
            h = pot_height + 0.25 + 0.08 * (i % 3)
            reach = 0.25 + 0.05 * (i % 2)
            rod((0, 0, pot_height), (reach * math.cos(a), reach * math.sin(a), h + 0.25), 0.006, m["leaf-dark"])
            ball(0.16, (reach * math.cos(a), reach * math.sin(a), h + 0.3), m["leaf"] if i % 2 else m["leaf-dark"],
                 scale=(1.0, 0.7, 0.12))
    elif kind == "ficus":
        rod((0, 0, pot_height), (0.03, 0.02, pot_height + 0.9), 0.025, m["walnut"])
        for i, (x, y, z, r) in enumerate([(0, 0, 1.45, 0.32), (0.18, 0.08, 1.3, 0.24), (-0.16, -0.1, 1.32, 0.22),
                                          (0.05, -0.15, 1.6, 0.2), (-0.08, 0.14, 1.62, 0.18)]):
            ball(r, (x, y, z), m["leaf"] if i % 2 else m["leaf-dark"], segments=16)
    else:
        cyl(0.07, 0.45, (0, 0, pot_height + 0.22), m["leaf-dark"], verts=12)
        cyl(0.04, 0.18, (0.1, 0, pot_height + 0.3), m["leaf-dark"], verts=12, rot=(0, math.radians(35), 0))
        cyl(0.035, 0.15, (-0.09, 0, pot_height + 0.2), m["leaf-dark"], verts=12, rot=(0, math.radians(-40), 0))


def rug_rect(m):
    mat = material("Rug", texture=image("rug", rug([srgb((120, 50, 44)), srgb((226, 214, 190)), srgb((52, 70, 104))])),
                   roughness=1.0)
    box((2.0, 1.4, 0.012), (0, 0, 0.006), mat, bevel=0.0)


def rug_round(m):
    mat = material("Round rug", texture=image("rug-round", rug([srgb((210, 196, 170)), srgb((150, 120, 90)), srgb((230, 222, 204))],
                                                           round_rug=True)), roughness=1.0)
    cyl(0.8, 0.012, (0, 0, 0.006), mat, verts=64)


def mirror_floor(m):
    box((0.6, 0.04, 1.7), (0, 0.1, 0.85), m["oak"], bevel=0.01, rot=(math.radians(-8), 0, 0))
    box((0.52, 0.01, 1.6), (0, 0.075, 0.86), m["mirror"], rot=(math.radians(-8), 0, 0))


def piano(m):
    black = m["ink"]
    box((1.5, 0.6, 1.25), (0, 0.05, 0.625), black, bevel=0.01)
    box((1.4, 0.3, 0.05), (0, -0.35, 0.72), black, bevel=0.005)
    box((1.3, 0.16, 0.02), (0, -0.33, 0.755), m["white"])
    for i in range(36):
        box((0.012, 0.09, 0.012), (-0.63 + i * 0.036, -0.3, 0.77), black)
    for sx in (-1, 1):
        box((0.06, 0.06, 0.66), (sx * 0.68, -0.4, 0.33), black)
    box((1.0, 0.35, 0.05), (0, -0.85, 0.48), black, bevel=0.01)
    legs(1.0, 0.35, 0.46, black, radius=0.02, inset=0.05)


def fireplace(m):
    surround = box((1.5, 0.4, 1.1), (0, 0.1, 0.55), m["white"], bevel=0.0)
    subtract(surround, box((0.9, 0.5, 0.7), (0, -0.12, 0.4), m["white"], bevel=0.0))
    box((0.9, 0.02, 0.7), (0, 0.18, 0.4), m["ink"], bevel=0.0)  # the soot-black back of the hearth
    box((0.9, 0.3, 0.02), (0, 0.04, 0.06), m["ink"], bevel=0.0)
    box((1.6, 0.45, 0.06), (0, 0.08, 1.13), m["marble"], bevel=0.006)
    for i in range(3):
        cyl(0.05, 0.6, (-0.15 + i * 0.1, 0.02, 0.12 + (i % 2) * 0.08), m["walnut"], rot=(0, math.pi / 2, math.radians(10 * i)),
            verts=12)


# ---------------------------------------------------------------------------------------------------------- the library

# id, name, category, against a wall, builder
PIECES = [
    ("chair-classic", "Silla clásica de roble", "seating", False, chair_classic),
    ("chair-windsor", "Silla Windsor", "seating", False, chair_windsor),
    ("chair-upholstered", "Silla tapizada de terciopelo", "seating", False, chair_upholstered),
    ("chair-scandi", "Silla nórdica", "seating", False, chair_scandi),
    ("chair-office", "Silla de oficina", "seating", False, chair_office),
    ("stool-bar", "Taburete alto", "seating", False, stool_bar),
    ("stool-low", "Taburete bajo", "seating", False, stool_low),
    ("bench-oak", "Banco de roble", "seating", False, bench),
    ("sofa-two-seat", "Sofá de dos plazas", "sofas", True, lambda m: sofa(m, 1.7, m["blue-fabric"], 2)),
    ("sofa-three-seat", "Sofá de tres plazas", "sofas", True, lambda m: sofa(m, 2.3, m["grey-fabric"], 3)),
    ("sofa-corner", "Sofá rinconera", "sofas", True, sofa_corner),
    ("sofa-chaise", "Sofá con chaise longue", "sofas", True, sofa_chaise),
    ("armchair-club", "Sillón club de cuero", "sofas", False, armchair_club),
    ("armchair-lounge", "Butaca de lectura", "sofas", False, armchair_lounge),
    ("pouf", "Puf", "sofas", False, pouf),
    ("table-dining-rect", "Mesa de comedor rectangular", "tables", False, table_rect),
    ("table-dining-round", "Mesa de comedor redonda", "tables", False, table_round),
    ("table-coffee", "Mesa de centro de mármol", "tables", False, table_coffee),
    ("table-side", "Mesa auxiliar", "tables", False, table_side),
    ("desk", "Escritorio con cajonera", "tables", True, desk),
    ("table-console", "Consola", "tables", True, table_console),
    ("bedside", "Mesilla de noche", "tables", True, bedside),
    ("bed-upholstered", "Cama doble tapizada", "beds", True, bed_upholstered),
    ("bed-king-walnut", "Cama king de nogal", "beds", True, bed_king),
    ("bed-iron", "Cama individual de hierro", "beds", True, bed_iron),
    ("bunk-bed", "Litera", "beds", True, bunk_bed),
    ("crib", "Cuna", "beds", True, crib),
    ("bookshelf", "Estantería con libros", "storage", True, bookshelf),
    ("sideboard", "Aparador de nogal", "storage", True, sideboard),
    ("dresser", "Cómoda de tres cajones", "storage", True, dresser),
    ("tv-unit", "Mueble de TV con televisor", "storage", True, tv_unit),
    ("wardrobe-sliding", "Armario de puertas correderas", "storage", True, wardrobe_sliding),
    ("shoe-cabinet", "Zapatero", "storage", True, shoe_cabinet),
    ("kitchen-island", "Isla de cocina con taburetes", "kitchen", False, kitchen_island),
    ("range-cooker", "Cocina con horno", "kitchen", True, range_cooker),
    ("dishwasher", "Lavavajillas", "kitchen", True, dishwasher),
    ("fridge-american", "Frigorífico americano", "kitchen", True, fridge_american),
    ("bathtub-freestanding", "Bañera exenta", "bathroom", False, bathtub_freestanding),
    ("vanity-double", "Mueble de lavabo doble con espejo", "bathroom", True, vanity_double),
    ("toilet-wall", "Inodoro suspendido", "bathroom", True, toilet_wall),
    ("towel-radiator", "Radiador toallero", "bathroom", True, towel_radiator),
    ("lamp-arc", "Lámpara de pie de arco", "lighting", False, lamp_arc),
    ("lamp-tripod", "Lámpara trípode", "lighting", False, lamp_tripod),
    ("lamp-globe", "Lámpara de pie de globo", "lighting", False, lamp_globe),
    ("lamp-reading", "Lámpara de lectura", "lighting", False, lamp_reading),
    ("plant-monstera", "Monstera en maceta", "decor", False, lambda m: plant(m, 0.2, 0.4, "monstera")),
    ("plant-ficus", "Ficus", "decor", False, lambda m: plant(m, 0.22, 0.45, "ficus")),
    ("plant-cactus", "Cactus", "decor", False, lambda m: plant(m, 0.15, 0.3, "cactus")),
    ("rug-rect", "Alfombra rectangular", "decor", False, rug_rect),
    ("rug-round", "Alfombra redonda", "decor", False, rug_round),
    ("mirror-floor", "Espejo de pie", "decor", True, mirror_floor),
    ("piano-upright", "Piano vertical con banqueta", "decor", True, piano),
    ("fireplace", "Chimenea", "decor", True, fireplace),
]


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def bounds():
    lo = np.array([1e9, 1e9, 1e9])
    hi = -lo
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        for corner in evaluated.bound_box:
            p = np.array(evaluated.matrix_world @ mathutils.Vector(corner))
            lo, hi = np.minimum(lo, p), np.maximum(hi, p)
    return lo, hi


def render_thumbnail(path):
    scene = bpy.context.scene
    engines = scene.render.bl_rna.properties["engine"].enum_items.keys()
    scene.render.engine = next((e for e in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT") if e in engines), "BLENDER_WORKBENCH")
    scene.render.resolution_x = THUMBNAIL
    scene.render.resolution_y = THUMBNAIL
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.8, 0.8, 0.82, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.9
    scene.world = world
    lo, hi = bounds()
    centre = (lo + hi) / 2
    radius = float(np.linalg.norm(hi - lo)) / 2
    direction = np.array([0.55, -1.0, 0.6])
    direction /= np.linalg.norm(direction)
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 50
    fov = 2 * math.atan(cam_data.sensor_width / (2 * cam_data.lens))
    distance = radius / math.sin(fov / 2) * 1.05
    camera = bpy.data.objects.new("Camera", cam_data)
    camera.location = tuple(centre + direction * distance)
    scene.collection.objects.link(camera)
    track = camera.constraints.new("TRACK_TO")
    target = bpy.data.objects.new("Target", None)
    target.location = tuple(centre)
    scene.collection.objects.link(target)
    track.target = target
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    scene.camera = camera
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 3.5
    sun = bpy.data.objects.new("Sun", sun_data)
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
    scene.collection.objects.link(sun)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def build(piece_id, builder):
    clear()
    looks = palette()
    builder(looks)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, piece_id + ".glb"), export_format="GLB", export_yup=True,
                              export_apply=True)
    lo, hi = bounds()
    render_thumbnail(os.path.join(OUT, piece_id + ".png"))
    triangles = 0
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            triangles += sum(len(p.vertices) - 2 for p in obj.data.polygons)
    return hi - lo, triangles


def main():
    os.makedirs(OUT, exist_ok=True)
    only = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    entries = []
    for piece_id, name, category, against, builder in PIECES:
        entry = {"id": piece_id, "name": name, "category": category, "file": piece_id + ".glb",
                 "thumbnail": piece_id + ".png", "againstWall": against, "credit": CREDIT}
        entries.append(entry)
        if only and piece_id not in only:
            continue
        size, triangles = build(piece_id, builder)
        print("F101LIB %s %.2f x %.2f x %.2f m, %d triangles" % (piece_id, size[0], size[1], size[2], triangles))
    with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8", newline="\n") as out:
        json.dump({"version": 1, "furniture": entries}, out, ensure_ascii=False, indent=2)
        out.write("\n")


main()
