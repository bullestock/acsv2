import cadquery as cq
import standoffs
from defs import *

led_cc = 10
balls = (cq.Workplane("XY")
         .tag("o")
         .transformed(offset=(-led_cc, 0, 0))
          .sphere(5/2)
          .workplaneFromTagged("o")
          .transformed(offset=(0, 0, 0))
          .sphere(5/2)
          .workplaneFromTagged("o")
          .transformed(offset=(led_cc, 0, 0))
          .sphere(5/2)
          )

result = (cq.Workplane("XY")
          .box(3*led_cc, led_cc, 2)
          .edges("|Z")
          .fillet(4.99)
          .faces(">Z")
          .fillet(1)
          )

result = result - balls.translate([0, 0, -1.5-0.45])

show_object(result)
