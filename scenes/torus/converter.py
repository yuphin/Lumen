# import os, sys

# #os.add_dll_directory('D:/Programs/Mitsuba 0.5.0/python/3.3')
# sys.path.append('D:/Programs/Mitsuba 0.5.0/python/3.3')
# os.environ['PATH'] = 'D:/Programs/Mitsuba 0.5.0' + os.pathsep + os.environ['PATH']
# import mitsuba
# from mitsuba.core import *
# from mitsuba.render import SceneHandler, RenderQueue, RenderJob
# import multiprocessing
# # Get a reference to the thread's file resolver
# fileResolver = Thread.getThread().getFileResolver()
# # Register any searchs path needed to load scene resources (optional)
# fileResolver.appendPath('.')
# # Optional: supply parameters that can be accessed
# # by the scene (e.g. as $myParameter)
# paramMap = StringMap()
# paramMap['myParameter'] = 'value'
# # Load the scene from an XML file
# scene = SceneHandler.loadScene(fileResolver.resolve("torus.xml"), paramMap)
# # Display a textual summary of the scene's contents
# scheduler = Scheduler.getInstance()
# # Start up the scheduling system with one worker per local core
# for i in range(0, multiprocessing.cpu_count()):
#     scheduler.registerWorker(LocalWorker(i, 'wrk%i' % i))
# scheduler.start()
# # Create a queue for tracking render jobs
# queue = RenderQueue()
# scene.setDestinationFile('renderedResult')
# # Create a render job and insert it into the queue
# job = RenderJob('myRenderJob', scene, queue)
# #job.start()
# #queue.waitLeft(0)
# #queue.join()
# # Print some statistics about the rendering process
# print(scene.shapes())

import os, sys
import mitsuba
mitsuba.set_variant('scalar_rgb')
from mitsuba.core import FileStream, Thread
from mitsuba.render import Integrator
from mitsuba.core.xml import load_file
filename = 'torus.xml'
# Add the scene directory to the FileResolver's search path
Thread.thread().file_resolver().append(os.path.dirname(filename))

# Load the scene for an XML file
scene = load_file(filename)
i = 0
for shape in scene.shapes():
    shape.write_ply(filename=os.path.join('.', 'mesh_%04i.ply' % (i)))
    i+=1