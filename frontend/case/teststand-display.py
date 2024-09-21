import cadquery as cq
import standoffs
import math
from defs import *

thickness = th

centerXY = (True, True, False)

width *= 0.6

result = (cq.Workplane("XY")
          .tag("o")
          .box(width, height, thickness, centered=centerXY)
          .edges("|Z")
          .fillet(2)
          .faces(">Z")
          .fillet(1)
          )

# hole for connector
result = (result
          .workplaneFromTagged("o")
          .transformed(offset=(0, height/2 - 5, 0))
          .rect(40, 10)
          .cutThruAll()
          )

# screwposts for display
def make_disp_screwpost(o, xs, ys):
    ovec1 = (xs*disp_h_cc_y/2, ys*disp_h_cc_x/2, 0)
    ovec2 = (xs*disp_h_cc_y/2, ys*disp_h_cc_x/2, 0)
    return (o
            .workplaneFromTagged("o")
            .transformed(offset=ovec1)
            .circle(2.5)
            .extrude(-3.5)
            .workplaneFromTagged("o")
            .transformed(offset=ovec2)
            .circle(1.25)
            .cutBlind(-10)
            )

result = make_disp_screwpost(result, -1, -1)
result = make_disp_screwpost(result, -1,  1)
result = make_disp_screwpost(result,  1, -1)
result = make_disp_screwpost(result,  1,  1)

result = (result
          .workplaneFromTagged("o")
          .transformed(offset=(-25, 0, 0))
          .rarray(1, 50, 1, 2)
          .circle(3.5/2)
          .cutThruAll()
          )
show_object(result)
