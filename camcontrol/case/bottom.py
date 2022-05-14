import cadquery as cq
import standoffs
from defs import *

print = True
#print = False

thickness = 42

standoff_h = 5
standoff_d = 7

screwpost_d = 10.1 # must be > 2*fillet_r
screwpost = standoffs.square_screwpost_body(screwpost_d, thickness-th, fillet_r)

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

# distribute standoffs for relay board
relay_standoff = standoffs.round_standoff(insert_r, insert_sr, insert_l, standoff_d, standoff_h)
relay_x_offset = 25
relay_y_offset = -30
relay_dx = 40
relay_dy = 38
relay_standoffs = (shell
                   .workplaneFromTagged("bottom")
                   # place standoffs on bottom
                   .transformed(offset=(relay_x_offset, relay_y_offset, th+standoff_h/2))
                   .rect(relay_dx, relay_dy, forConstruction=True)
                   .vertices()
                   .eachpoint(lambda loc: relay_standoff.val().moved(loc), True)
                   )

# distribute standoffs for main board
main_standoff = standoffs.round_standoff(3.25/2, 2.75/2, 6.2, standoff_d, standoff_h)
main_x_offset = 25
main_y_offset = 20
main_dx = 65.532
main_dy = 45.974
main_standoffs = (shell
                  .workplaneFromTagged("bottom")
                  # place standoffs on bottom
                  .transformed(offset=(main_x_offset, main_y_offset, th+standoff_h/2))
                  .rect(main_dx, main_dy, forConstruction=True)
                  .vertices()
                  .eachpoint(lambda loc: main_standoff.val().moved(loc), True)
                  )

# distribute screwposts and holes
screwposts = (shell
              .workplaneFromTagged("bottom")
              .transformed(offset=(0, 0, (th+thickness)/2))
              .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
              .vertices()
              .eachpoint(lambda loc: screwpost.val().moved(loc), True)
              )

# combine
result = shell.union(relay_standoffs).union(main_standoffs).union(screwposts)
#result = result - opz.opi_jacks_cutter(opi_x_offset, opi_y_offset, th+standoff_h)

# screw holes
result = (result
          .workplaneFromTagged("bottom")
          .transformed(offset=(0, 0, (th+thickness)/2))
          .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
          .vertices()
          .circle(insert_sr+.25)
          .cutThruAll()
          )
result = (result
          .workplaneFromTagged("bottom")
          .transformed(offset=(0, 0, 0))
          .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
          .vertices()
          .circle(screw_head_r)
          .cutBlind(screw_head_h)
          )

# holes for wall fitting
result = (result
          .workplaneFromTagged("bottom")
          .transformed(offset=(0, 25, 0))
          .rarray(wh_dist, 1, 2, 1)
          .circle(3.5/2).cutThruAll()
          )

#result = (result.faces(">Y").workplane(-38).split(keepBottom=True))
show_object(result)
