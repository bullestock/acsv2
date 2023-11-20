import cadquery as cq

from door_defs import *
from door_defs_small import *

plate_d = 2
eps = 0.1

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
         )

outer = (cq.Workplane("XY")
         .box(iw - eps, ih - eps, case_d - front_th, centered=centerXY)
         .edges("|Z")
         .fillet(5)
         .faces(">Z")
         .shell(-1)
         )

res = outer+ plate

show_object(res)
#show_object(scutout)
