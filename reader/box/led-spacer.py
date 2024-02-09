import cadquery as cq

from door_defs import *

res = (cq.Workplane("XY")
       .tag("base")
       .circle(5/2)
       .extrude(5)
       .workplaneFromTagged("base")
       .rarray(1, 2.54, 1, 2)
       .circle(1.5/2)
       .cutThruAll()
       )

show_object(res)

