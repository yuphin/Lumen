import sys
import os
import glob
import subprocess

path = '.'

shaderfiles = []
for exts in ('*.vert', '*.frag', '*.comp', '*.geom', '*.tesc', '*.tese'):
	shaderfiles.extend(glob.glob(os.path.join(path, exts)))

failedshaders = []

if not os.path.exists('spv'):
    os.makedirs('spv')

for shaderfile in shaderfiles:
		print("\n-------- %s --------\n" % shaderfile)
		if subprocess.call("glslangValidator.exe -V %s -o spv/%s.spv" % (shaderfile, shaderfile), shell=True) != 0:
			failedshaders.append(shaderfile)

print("\n-------- Compilation result --------\n")

if len(failedshaders) == 0:
	print("SUCCESS: All shaders compiled to SPIR-V")
else:
	print("ERROR: %d shader(s) could not be compiled:\n" % len(failedshaders))
	for failedshader in failedshaders:
		print("\t" + failedshader)
