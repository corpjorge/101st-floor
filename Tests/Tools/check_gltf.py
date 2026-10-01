# Imports a .glb exported by 101st Floor into Blender (headless) as a second opinion on the file (ADR-0020): prints what
# Blender read (objects, materials, images, bounds) and renders a picture of it with Eevee.
# Run through Scripts/CheckGltf.ps1:  blender -b --factory-startup --python check_gltf.py -- <file.glb> <picture.png>
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
source, picture = args[0], args[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=source)

meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
materials = sorted({slot.material.name for o in meshes for slot in o.material_slots if slot.material})
images = [i for i in bpy.data.images if i.size[0] > 0]
low = Vector((math.inf, math.inf, math.inf))
high = -low
triangles = 0
for o in meshes:
    for corner in o.bound_box:
        world = o.matrix_world @ Vector(corner)
        low = Vector(map(min, low, world))
        high = Vector(map(max, high, world))
    o.data.calc_loop_triangles()
    triangles += len(o.data.loop_triangles)
print("F101GLTF objects=%d materials=%d images=%d triangles=%d" % (len(meshes), len(materials), len(images), triangles))
print("F101GLTF bounds min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)" % (*low, *high))
for o in bpy.context.scene.objects:
    if o.parent is None:
        print("F101GLTF root %s" % o.name)
for name in materials:
    print("F101GLTF material %s" % name)
for image in images:
    print("F101GLTF image %s %dx%d" % (image.name, image.size[0], image.size[1]))

# A picture from the south-west, above, without the slabs (extras f101Source, kept by the importer as a custom property)
# so the rooms can be seen: the whole model in view, lit by a sun and a soft world.
hidden = 0
for o in meshes:
    if o.get("f101Source") == "slab":
        o.hide_render = True
        hidden += 1
print("F101GLTF slabs hidden in the picture: %d" % hidden)
centre = (low + high) / 2
size = max((high - low).length, 1.0)
camera_data = bpy.data.cameras.new("F101Camera")
camera = bpy.data.objects.new("F101Camera", camera_data)
bpy.context.scene.collection.objects.link(camera)
direction = Vector((-0.55, -0.75, 0.9)).normalized()
camera.location = centre + direction * size * 1.15
camera.rotation_euler = (centre - camera.location).to_track_quat("-Z", "Y").to_euler()
camera_data.lens = 32
bpy.context.scene.camera = camera
sun_data = bpy.data.lights.new("F101Sun", "SUN")
sun_data.energy = 3.0
sun_data.angle = math.radians(3)
sun = bpy.data.objects.new("F101Sun", sun_data)
sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
bpy.context.scene.collection.objects.link(sun)
world = bpy.data.worlds.new("F101World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.68, 0.76, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
bpy.context.scene.world = world
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
scene.render.resolution_x = 1280
scene.render.resolution_y = 800
scene.render.filepath = picture
bpy.ops.render.render(write_still=True)
print("F101GLTF picture %s" % picture)
