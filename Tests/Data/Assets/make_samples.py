# Sample assets for the import tests (ADR-0017), made with Blender's own glTF and OBJ exporters so the readers are checked
# against real exporter output. Regenerate with:
#   & "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python Tests/Data/Assets/make_samples.py
# Writes, next to this script: armchair.glb (two materials, one with a texture packed inside), lamp.obj + lamp.mtl +
# lamp_shade.png (a textured shade), parquet.png and stone.jpg (tiling textures).
import math
import os

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def image(name, size, pixels, path=None, file_format="PNG"):
    img = bpy.data.images.new(name, width=size, height=size, alpha=False)
    img.pixels.foreach_set(pixels.astype(np.float32).ravel())
    if path:
        img.filepath_raw = path
        img.file_format = file_format
        img.save()
    return img


def planks(size):
    # Oak-coloured planks: rows of boards with staggered joints and a soft grain (linear colour, rows from the bottom).
    y, x = np.mgrid[0:size, 0:size] / size
    row = np.floor(y * 6)
    offset = (row * 0.37) % 1.0
    along = (x + offset) % 1.0
    board = np.floor(along * 2)
    tone = 0.85 + 0.15 * np.sin(row * 12.9898 + board * 78.233) ** 2
    grain = 0.06 * np.sin((y * 6 % 1.0) * 40 + np.sin(along * 25) * 2)
    joint = ((y * 6 % 1.0) < 0.03) | ((along * 2 % 1.0) < 0.01)
    base = np.stack([0.42, 0.22, 0.09]) [:, None, None] * (tone + grain)
    base = np.where(joint[None], 0.08, base)
    rgba = np.concatenate([base, np.ones((1, size, size))])
    return np.transpose(rgba, (1, 2, 0))


def stones(size):
    # Grey flagstones with light mortar.
    y, x = np.mgrid[0:size, 0:size] / size
    cell = np.floor(x * 4) + np.floor(y * 4) * 4
    tone = 0.25 + 0.12 * np.sin(cell * 43.7) ** 2 + 0.04 * np.sin(x * 90) * np.sin(y * 70)
    mortar = ((x * 4 % 1.0) < 0.04) | ((y * 4 % 1.0) < 0.04)
    grey = np.where(mortar, 0.55, tone)
    rgba = np.stack([grey, grey * 0.98, grey * 0.94, np.ones_like(grey)])
    return np.transpose(rgba, (1, 2, 0))


def stripes(size):
    # Linen with stripes, for the lamp shade.
    y, x = np.mgrid[0:size, 0:size] / size
    stripe = (np.floor(x * 12) % 2) == 0
    rgba = np.stack([np.where(stripe, 0.85, 0.55), np.where(stripe, 0.8, 0.35), np.where(stripe, 0.7, 0.2), np.ones_like(x)])
    return np.transpose(rgba, (1, 2, 0))


def material(name, color, roughness, texture=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    if texture is not None:
        node = mat.node_tree.nodes.new("ShaderNodeTexImage")
        node.image = texture
        mat.node_tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
    return mat


def box(name, size, location, mat):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.shade_flat()
    bpy.ops.mesh.uv_texture_add()
    obj.data.materials.append(mat)
    return obj


def cylinder(name, radius, depth, location, mat, vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.data.materials.append(mat)
    return obj


def armchair():
    # 80 cm wide, 75 cm deep, 85 cm tall; its front towards Blender's -Y (glTF +Z).
    clear()
    fabric = material("Upholstery", (1, 1, 1), 0.9, image("fabric", 128, stripes(128)))
    wood = material("Wood", (0.25, 0.12, 0.05), 0.5)
    box("Seat", (0.8, 0.75, 0.2), (0, 0, 0.35), fabric)
    box("Back", (0.8, 0.15, 0.45), (0, 0.3, 0.675), fabric)
    for side in (-1, 1):
        box("Arm", (0.12, 0.75, 0.25), (side * 0.34, 0, 0.575), fabric)
        for depth in (-1, 1):
            cylinder("Leg", 0.025, 0.25, (side * 0.35, depth * 0.3, 0.125), wood, 12)
    bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, "armchair.glb"), export_format="GLB", export_yup=True,
                              export_apply=True)


def lamp():
    # A floor lamp 1.6 m tall with a striped shade, exported as OBJ with its MTL and texture next to it.
    clear()
    metal = material("Metal", (0.6, 0.6, 0.62), 0.3)
    shade = material("Shade", (1, 1, 1), 0.8, image("lamp_shade", 128, stripes(128), os.path.join(HERE, "lamp_shade.png")))
    cylinder("Base", 0.15, 0.03, (0, 0, 0.015), metal)
    cylinder("Pole", 0.015, 1.3, (0, 0, 0.68), metal, 12)
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.25, radius2=0.15, depth=0.3, location=(0, 0, 1.45), end_fill_type="NOTHING")
    cone = bpy.context.active_object
    cone.name = "Shade"
    cone.data.materials.append(shade)
    bpy.ops.wm.obj_export(filepath=os.path.join(HERE, "lamp.obj"), export_materials=True, path_mode="RELATIVE",
                          forward_axis="NEGATIVE_Z", up_axis="Y")


def textures():
    clear()
    image("parquet", 256, planks(256), os.path.join(HERE, "parquet.png"))
    image("stone", 256, stones(256), os.path.join(HERE, "stone.jpg"), "JPEG")


armchair()
lamp()
textures()
print("samples written to", HERE)
