import cadquery as cq
import standoffs
from defs import *

standoff_h = 5
standoff_d = 8

# pcb height 18 mm above standoffs
thickness = th + 40

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
relay_x_offset = 17
relay_y_offset = -32
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
main_x_offset = 30
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

pwr_z = 23
pwr_dy = -5
# power
result = (result
          # female plug part
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy, pwr_z, 0))
          .rect(24, 32)
          .cutBlind(-10)
          # ridge
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy + 1, pwr_z, 0))
          .rect(3, 34)
          .cutBlind(-10)
          # male plug part
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy + 24 + 1, pwr_z, 0))
          .rect(26, 28)
          .cutBlind(-10)
          # latches
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy + 13, pwr_z, 0))
          .rarray(1, 19, 1, 2)
          .rect(51, 5)
          .cutBlind(-10)
          # horizontal ridges
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy + 13, pwr_z, 0))
          .rarray(1, 10, 1, 2)
          .rect(52, 2)
          .cutBlind(-10)
          # ridge
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy + 26 + 2, pwr_z, 0))
          .rect(3, 34)
          .cutBlind(-10)
          # recess
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-pwr_dy + 13, pwr_z, -2))
          .rarray(1, 19, 1, 2)
          .rect(58, 7)
          .cutBlind(-2)
          )

# antenna hole
result = (result
          .faces(">Y")
          .workplane(origin=(main_x_offset - 22.5, 0, 0))
          .transformed(offset=(0, thickness*0.75, 0))
          .circle(6.5/2)
          .cutBlind(-10)
         )

# DC jack hole
result = (result
          .faces(">Y")
          .workplane(origin=(main_x_offset + 22.5, 0, 0))
          .transformed(offset=(0, thickness*0.75, 0))
          .circle(8/2)
          .cutBlind(-10)
         )

# AV plug hole
result = (result
          .faces(">Y")
          .workplane(origin=(main_x_offset, 0, 0))
          .transformed(offset=(0, thickness*0.75, 0))
          .circle(11.7/2)
          .cutBlind(-10)
         )

# holes for wall fitting
result = (result
          .workplaneFromTagged("bottom")
          .transformed(offset=(-55, 0, 0))
          .rarray(1, wh_dist, 1, 2)
          .circle(3.5/2).cutThruAll()
          )

hth = 2
hilink = (cq.Workplane("XY")
         .transformed(offset=(-40, 30, th))
         .box(59 + 2*hth, 33 + 2*hth, 5, centered=centerXY)
         .faces(">Z")
         .rect(58, 33)
         .cutThruAll()
         )

dsn = (cq.Workplane("XY")
       .transformed(offset=(-20, -10, th))
         .box(12.5 + 2*hth, 19 + 2*hth, 5, centered=centerXY)
         .faces(">Z")
         .rect(12.5, 19)
         .cutThruAll()
         )

result = result + hilink + dsn

show_object(result)
