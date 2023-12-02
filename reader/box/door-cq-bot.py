import cadquery as cq

from door_defs import *

plate_d = 2
eps = 0.1
hole_offset = 0

centerXY = (True, True, False)

plate = (cq.Workplane("XY")
         .tag("base")
         .box(case_w, case_h, plate_d, centered=centerXY)
         .edges("|Z")
         .fillet(7.5)
         # screw holes
         .workplaneFromTagged("base")
         .rarray(sh_dist, 1, 2, 1)
         .circle(sh_dia/2)
         .cutThruAll()
         .workplaneFromTagged("base")
         .transformed(offset=(0, 0, 0))
         .circle(15)
         .cutThruAll()
         )

outer = (cq.Workplane("XY")
         .box(iw - eps, ih - eps, case_d - front_th, centered=centerXY)
         .edges("|Z")
         .fillet(5)
         .faces(">Z")
         .shell(-1)
         )

res = outer + plate

show_object(res)
#show_object(scutout)
