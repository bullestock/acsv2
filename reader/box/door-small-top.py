import cadquery as cq

from door_defs_small import *

centerXY = (True, True, False)

outer = (cq.Workplane("XY")
         .tag("base")
         .box(case_w, case_h, case_d, centered=centerXY)
         .edges(">Z or |Z")
         .fillet(7.5)
         # screw holes
         .workplaneFromTagged("base")
         .rarray(sh_dist, 1, 2, 1)
         .circle(sh_dia/2)
         .cutThruAll()
         # LED holes
         .workplaneFromTagged("base")
         .transformed(offset=(led_offset, 0, 0))
         .rarray(1, led_dist, 1, 2)
         .circle(led_dia/2)
         .cutThruAll()
         )

inner = (cq.Workplane("XY")
         .box(iw, ih, case_d - front_th, centered=centerXY)
         .edges("|Z")
         .fillet(5)
         )

scutout1 = (cq.Workplane("XY")
           .transformed(offset=(case_w/2 - sc_w/2, 0, case_d - sc_d))
           .box(sc_w, sc_w, sc_d, centered=centerXY)
           )
scutout2 = (cq.Workplane("XY")
           .transformed(offset=(-case_w/2 + sc_w/2, 0, case_d - sc_d))
           .box(sc_w, sc_w, sc_d, centered=centerXY)
           )

res = outer-inner - scutout1 - scutout2

show_object(res)
#show_object(scutout)
