import cadquery as cq
from defs import *

cover_th = 2
plug_w = 9
cover_w = plug_w + 3
plug_front_th = 3
cover_h = 23.3 - (42 - 28)

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
