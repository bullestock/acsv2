import cadquery as cq
import orangepizero as opz
import standoffs
from defs import *

print = True
#print = False

#!!
thickness = 31
opi_x_offset = -17
opi_y_offset = -5

# rotated
holes_dx = 42.11087
holes_dy = 40.1109

# opi standoff dimensions
standoff_h = 5
standoff_d = 7

standoff = standoffs.round_standoff(standoff_d, standoff_h)

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

# distribute standoffs
standoffs = (shell
             .workplaneFromTagged("bottom")
             # place standoffs on bottom
             .transformed(offset=(opi_x_offset, opi_y_offset, th+standoff_h/2))
             .rect(holes_dx, holes_dy, forConstruction=True)
             .vertices()
             .eachpoint(lambda loc: standoff.val().moved(loc), True)
             )

# distribute screwposts and holes
screwposts = (shell
              .workplaneFromTagged("bottom")
              .transformed(offset=(0, 0, (th+thickness)/2))
              .rect(width - 1.2*screwpost_d, height - 1.2*screwpost_d, forConstruction=True)
              .vertices()
              .eachpoint(lambda loc: screwpost.val().moved(loc), True)
              )

# opi
opi = opz.opi(shell
              .workplaneFromTagged("bottom")
              .transformed(offset=(opi_x_offset,
                                   opi_y_offset,
                                   th+standoff_h)))

# smps
smps_l = 30.1
smps_w = 18.5
smps_h = 7.5
smps_wall_h = 4
smps_wall_th = 2.5
smps_y_offset = 35

if print:
    ex = 2*smps_wall_th
    smps = (cq.Workplane(origin=(0, 0, 0))
            .transformed(offset=(-1, smps_y_offset, th))
            .tag("base")
            .box(smps_l+ex, smps_w+ex, smps_wall_h, centered=centerXY)
            .faces("|Z")
            .shell(-smps_wall_th)
            # add holes in corners
            .workplaneFromTagged("base")
            .rect(smps_l, smps_w, forConstruction=True)
            .vertices()
            .circle(1).cutBlind(smps_wall_h)
            )
else:
    smps = (cq.Workplane()
            .transformed(offset=(0, smps_y_offset, th))
            .box(smps_l, smps_w, smps_h, centered=centerXY)
          )

# combine
result = shell.union(standoffs).union(screwposts).union(smps)
if not print:
    result = result.union(opi)

result = opz.opi_jacks_cut(result,
                           opi_x_offset, opi_y_offset, th+standoff_h)
    
#!! avplug cutout (leave switch, 12 V out)

#!! dc jack

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
          .transformed(offset=(0, 20, 0))
          .rarray(wh_dist, 1, 2, 1)
          .circle(3.5/2).cutThruAll()
          )

#result = (result.faces(">Y").workplane(-38).split(keepBottom=True))
show_object(result)
