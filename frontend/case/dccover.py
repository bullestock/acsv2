import cadquery as cq
from defs import *

thickness = 42
plug_h = 10.8+0.7
plug_d = 8
cover_th = 2
plug_w = 9.2
cover_w = plug_w + 3
plug_front_th = 3
cover_h = 23.3

result = (cq.Workplane("XY")
          # plate 
          .rect(cover_w-0.2, cover_th-0.4)
          .extrude(cover_h)
          # cover
          .transformed(offset=(0, cover_th, 0))
          .rect(plug_w-0.05, plug_front_th)
          .extrude(cover_h)
          )

show_object(result)

box = (cq.Workplane("XY")
       .box(17.45, 1, 3, centered=(True, True, False))
       .transformed(offset=(0, 3.5-1/2, 3))
       .box(17.45, 7, 2, centered=(True, True, False))
       )
#show_object(box)
