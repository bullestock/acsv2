import cadquery as cq
import standoffs
from defs import *

thickness = lid_h
led_cc = 10
disp_w, disp_h = 23, 12

screwpost_d = 10.1 # must be > 2*fillet_r
screwpost = standoffs.square_screwpost_nut(screwpost_d, thickness-th, fillet_r)

centerXY = (True, True, False)

# make shell
shell = (cq.Workplane("XY")
         .box(height, width, thickness, centered=centerXY)
         .faces(">Z")
         .shell(-th)
         # round edges
          .edges("<Z or |Z").fillet(fillet_r)
         )
shell.faces("<Z").workplane(centerOption="CenterOfMass", 
                            invert=True).tag("top")

# distribute screwposts
screwposts = (shell
              .workplaneFromTagged("top")
              .transformed(offset=(0, 0, (th+thickness)/2))
              .rect(height - 1.2*screwpost_d, width - 1.2*screwpost_d, forConstruction=True)
              .vertices()
              .eachpoint(lambda loc: screwpost.val().moved(loc), True)
              )

button_x = -(43+13)/2
button_y = -40
disp_x = 25

# button cutout
result = (shell
          .faces("<Z")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(button_x, button_y, 0))
          .circle(6.25)
          .cutBlind(-10)
          )

# display cutout

disp_wth = th
centerXY = (True, True, False)

disphole = (cq.Workplane("XY")
            .transformed(offset=(disp_x, -button_y, 0))
            .box(disp_w+2*disp_wth, disp_h+2*disp_wth, disp_wth, centerXY)
            .edges(">Z")
            .chamfer(disp_wth*0.99)
            )

result = result.cut(disphole)

# combine
result = result.union(screwposts)

result = (result
          .faces(">Z")
          .rect(height - 1.2*screwpost_d, width - 1.2*screwpost_d, forConstruction=True)
          .vertices()
          .circle(insert_r)
          .cutBlind(lid_h - 2)
          )

show_object(result)
