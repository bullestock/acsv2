import cadquery as cq
from defs import *

r1 = 6.5
r2 = 7.5
r3 = r2 + 1.5
d = 2

result = (cq.Workplane("XY")
          .circle(r2)
          .extrude(4)
          .faces(">Z")
          .circle(r3)
          .extrude(1)
          .faces(">Z")
          .circle(r3)
          .workplane(d*0.75)
          .circle(r3-d)
          .loft()
          .circle(r1)
          .cutThruAll()
          )

show_object(result)
