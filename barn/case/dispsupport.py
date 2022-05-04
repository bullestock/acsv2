import cadquery as cq
from defs import *

mh_cc = 50
total_h = 31+2.6
foot_hole_d = 3.5
disp_hole_d = 2.5
dhx_cc = 30.48
dhy_cc = 28.448
width = 12
th = 3

result = (cq.Workplane("XY")
          .tag("base")
          # bar
          .box(mh_cc+width, width, th, centered=(True, True, False))
          # holes for case screws
          .transformed(offset=(disp_x_offset, 0, 0))
          .rarray(mh_cc, 1, 2, 1)
          .slot2D(foot_hole_d*2, foot_hole_d, 90)
          .cutThruAll()
         )
offset = 2

result = (result
          # display mount plate
          .workplaneFromTagged("base")
          .transformed(offset=(0, -2, 0))
          .box(36, 34, th, centered=(True, True, False))
          .edges("|Z")
          .fillet(1)
          # supports for display
          .workplaneFromTagged("base")
          .transformed(offset=(0, -2, th))
          .rect(dhx_cc, dhy_cc, forConstruction=True)
          .vertices()
          .circle(disp_hole_d)
          .extrude(2)
          # holes for display screws
          .workplaneFromTagged("base")
          .transformed(offset=(0, -2, th))
          .rect(dhx_cc, dhy_cc, forConstruction=True)
          .vertices()
          .circle(disp_hole_d/2)
          .cutThruAll()
          # cutout for display connector
          .workplaneFromTagged("base")
          .transformed(offset=(0, 15, 0))
          .rect(15, 15)
          .cutThruAll()
          # cutout for power connector
          .workplaneFromTagged("base")
          .transformed(offset=(22, 15, 0))
          .rect(15, 15)
          .cutThruAll()
          )

show_object(result)
