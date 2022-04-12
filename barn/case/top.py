import cadquery as cq
import standoffs
from defs import *

thickness = 8+th

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
disp_y_offset = 19

disphole = (cq.Workplane("XY")
            .transformed(offset=(0, disp_y_offset, 0)) #!!
            .box(25.5+2*th, 14+2*th, th, centerXY)
            .edges(">Z")
            .chamfer(th*0.99)
            )
result = result.cut(disphole)

# button hole
button_disp_dist = 8.75+3.2+14/2+1
log(button_disp_dist)
result = (result
          .workplane()
          .transformed(offset=(0, disp_y_offset-button_disp_dist, 0))
          .slot2D(16.5, 8.5)
          .cutThruAll()
         )

show_object(result)

