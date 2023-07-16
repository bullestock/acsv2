import cadquery as cq
import standoffs
from defs import *

thickness = lid_h
led_cc = 10

screwpost_d = 10.1 # must be > 2*fillet_r
screwpost = standoffs.square_screwpost_nut(screwpost_d, thickness-th, fillet_r)

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
                            invert=True).tag("top")

# distribute screwposts
screwposts = (shell
              .workplaneFromTagged("top")
              .transformed(offset=(0, 0, (th+thickness)/2))
              .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
              .vertices()
              .eachpoint(lambda loc: screwpost.val().moved(loc), True)
              )

center_x = -(43+13)/2
but_cc = 25
but_y = -40
led_y = -22

# button cutout
result = (shell
          .faces("<Z")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(center_x, but_y, 0))
          .circle(8)
          .cutBlind(-10)
          )

# LED cutouts
result = (result
          .faces("<Z")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(center_x - led_cc, led_y, 0))
          .circle(5/2)
          .cutThruAll()
          .faces("<Z")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(center_x, led_y, 0))
          .circle(5/2)
          .cutThruAll()
          .faces("<Z")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(center_x + led_cc, led_y, 0))
          .circle(5/2)
          .cutThruAll()
          )

# combine
result = shell.union(screwposts)

result = (result
          .faces(">Z")
          .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
          .vertices()
          .circle(insert_r)
          .cutBlind(-insert_l)
          )

show_object(result)
