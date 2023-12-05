import cadquery as cq
import orangepizero as opz
import standoffs
from defs import *

pcb_x_offset = 0
pcb_y_offset = -5.5

# rotated
holes_dx = 98 - 42
holes_dy = 106 - 65

# pcb standoff dimensions
standoff_h = 5
standoff_d = 7

# bottom thickness, standoffs, pcb, 10 mm free for buttons
thickness = th + standoff_h + 10 + 10
log(thickness)

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
             .transformed(offset=(pcb_x_offset, pcb_y_offset, th+standoff_h/2))
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

# combine
result = shell.union(standoffs).union(screwposts)

# pcb
'''
result = (result
          .workplaneFromTagged("bottom")
          .transformed(offset=(pcb_x_offset, pcb_y_offset, th+standoff_h))
          .rect(64, 50)
          .extrude(1.6)
          )
'''

avplug_z = 15

# avplug1 cutout (leave switch, 12 V out)
result = (result
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-28, avplug_z, 0))
          .circle(16.1/2)
          .cutBlind(-1)
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-28, avplug_z, 0))
          .circle(15/2)
          .extrude(-1)
          )

result = (result
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-28, avplug_z, 0))
          .circle(12.1/2)
          .cutBlind(-10)
          )

# avplug2 cutout (reader)
result = (result
          .faces("<X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(-2, avplug_z, 0))
          .circle(16.1/2)
          .cutBlind(-10)
          )

ant_z = 15

# antenna cutout
result = (result
          .faces(">X")
          .workplane(origin=(0, 0, 0))
          .transformed(offset=(20, ant_z, 0))
          .circle(6.5/2)
          .cutBlind(-10)
          )

# DC jack
plug_l = 14
plug_w = 9.2
block_w = 12
plug_h = 10.8+0.7
plug_h1 = 6.25
plug_d = 8
plug_front_th = 3
plug_front_offset = 0.65
h1 = 12
xpos = 32
ypos = (height/2-plug_l/2-th/2)
slot_th = 2
cover_th = 2
cover_w = plug_w + 3
cover_block_w = cover_w + 3
result = (result
          # support block
          .workplaneFromTagged("bottom")
          .transformed(offset=(xpos, ypos, th))
          .rect(block_w, plug_l).extrude(h1)
          # block for holding cover
          .workplaneFromTagged("bottom")
          .transformed(offset=(xpos, (height-th-cover_th)/2-slot_th, th))
          .rect(cover_block_w, 2*cover_th).extrude(thickness-th)
          # cylindrical cutout
          .workplaneFromTagged("bottom")
          .transformed(offset=(xpos, ypos+10, h1),
                       rotate=(90, 0, 0))
          .circle(plug_d/2).cutBlind(25)
          # square cutout
          .workplaneFromTagged("bottom")
          .transformed(offset=(xpos, ypos, h1))
          .rect(plug_w, 15).cutBlind(plug_h1+th+3)
          # front cutout
          .workplaneFromTagged("bottom")
          .transformed(offset=(xpos, 
                               (height-plug_front_th)/2, 
                               h1-plug_d/2-plug_front_offset))
          .rect(plug_w,
                plug_front_th)
          .cutBlind(thickness)
          # cover cutout
          .workplaneFromTagged("bottom")
          .transformed(offset=(xpos, (height-th)/2-slot_th, h1+plug_d))
          .rect(cover_w, cover_th).cutBlind(thickness-th-plug_d/2)
)

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
