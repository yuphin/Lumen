import argparse
import fileinput
import os
import subprocess
import sys

print('Compiling and disassembling shaders...')
for root, dirs, files in os.walk("."):
    path = root.split(os.sep)
    print((len(path) - 1) * '---', os.path.basename(root))
    for file in files:
         if file.endswith(".vert") or file.endswith(".frag") or file.endswith(".comp") or \
            file.endswith(".geom") or file.endswith(".tesc") or file.endswith(".tese") or \
            file.endswith(".rgen") or file.endswith(".rchit") or file.endswith(".rmiss"):
            input_file = os.path.join(root, file)
            output_file = os.path.join(root, file + '.spv')
            dis_output_file = os.path.join(root, file + '.spv.as')
            subprocess.call("%s -V %s --target-env vulkan1.3 -o %s" % ('glslangValidator.exe', input_file, output_file), shell=True)
            subprocess.call("%s %s -o %s" % ('spirv-dis.exe', output_file, dis_output_file), shell=True)
            #print(os.path.join(root, file))
            # Compile and disassemble
