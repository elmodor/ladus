import bpy
import bmesh
import mathutils
import code
import re
import os

name = os.path.basename(bpy.data.filepath)
name = re.sub("\.blend$", ".model", name)
out = open(name, "w")



def write(s):
	out.write(s)
	#print(s, end="")

scene = bpy.context.scene

for o in scene.objects:
	if o.type != 'MESH': continue

	write("mesh %s\n" % o.name)

	if o.active_material:
		col = o.active_material.diffuse_color
	else:
		col = mathutils.Color((0.5, 0.5, 0.5))

	write("color %d %d %d\n" % (col.r * 255, col.g * 255, col.b * 255))


	# apply modifiers
	for mod in o.modifiers:
		try:
			bpy.ops.object.modifier_apply(modifier=mod.name)
		except RuntimeError:
			import traceback
			traceback.print_exc()


	bm = bmesh.new()
	bm.from_mesh(o.data)

	# triangulate
	bmesh.ops.triangulate(bm, faces=bm.faces)

	# transform
	bm.transform(o.matrix_world)

#	for v in bm.verts:
#		write("vertex %g %g %g\n" % (v.co.x, v.co.z, -v.co.y))
#		write("normal %g %g %g\n" % (v.normal.x, v.normal.z, -v.normal.y))


	for f in bm.faces:
		assert len(f.verts) == 3, "mesh not triangulated"

#		write("triangle")
#		for v in f.verts: write(" %d" % v.index)
#		write("\n")

		for v in f.verts:
			write("vertex %g %g %g\n" % (v.co.x, v.co.z, -v.co.y))
			# calculate normal for fake ambient light
			n = (v.normal + f.normal * 2).normalized()
			write("normal %g %g %g\n" % (n.x, n.z, -n.y))

#			write("normal %g %g %g\n" % (v.normal.x, v.normal.z, -v.normal.y))
#			write("normal %g %g %g\n" % (f.normal.x, f.normal.z, -f.normal.y))



#	# animation
#	for i in range(scene.frame_start, scene.frame_end + 1):
#		scene.frame_set(i)
#		loc = mat.to_translation()
#		rot = mat.to_euler("XZY")
#		scale = mat.to_scale()
#		write("frame %g %g %g %g %g\n" % (
#			loc[0] * 10, loc[1] * -10,
#			-rot[2],
#			abs(scale[0]), abs(scale[1])
#		))

out.close()

#code.interact(local=locals())
