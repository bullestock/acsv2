import cadquery as cq
import standoffs
from defs import *

thickness = 9.75+th

screwpost_d = 10.1 # must be > 2*fillet_r
screwpost = standoffs.square_screwpost_nut(screwpost_d, thickness-th, 2)

centerXY = (True, True, False)

# make shell
shell = (cq.Workplane("XY")
         .box(width, height, thickness, centered=centerXY)
         .faces(">Z")
         .shell(-th)
         # round edges
         .edges("<Z or |Z").fillet(fillet_r)
        )
shell.faces("<Z").workplane(centerOption="CenterOfMass", 
                            invert=True).tag("bottom")

# distribute screwposts
screwposts = (shell
              .workplaneFromTagged("bottom")
              .transformed(offset=(0, 0, (th+thickness)/2))
              .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
              .vertices()
              .eachpoint(lambda loc: screwpost.val().moved(loc), True)
              )

# combine
result = shell.union(screwposts)

# display hole
disp_y_offset = 20.5
disp_x_offset = 1

disphole = (cq.Workplane("XY")
            .transformed(offset=(disp_x_offset, disp_y_offset, 0))
            .box(32+2*th, 17+2*th, th, centerXY)
            .edges(">Z")
            .chamfer(th*0.99)
            )
result = result.cut(disphole)

# button hole
result = (result
          .workplane()
          .transformed(offset=(0.5, -2.45, 0))
          .slot2D(16.5, 8.5)
          .cutThruAll()
         )
show_object(result)

