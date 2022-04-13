import cadquery as cq
import standoffs
import math
from defs import *

thickness = 8+th

centerXY = (True, True, False)

top_depth = 30
mid_depth = 40
bot_depth = 20
bend_x = 0.7*height

# make shell
result = (cq.Workplane("XZ")
          .hLine(height)
          .vLine(bot_depth)
          .lineTo(bend_x, mid_depth)
          .lineTo(0, top_depth)
          .close()
          .extrude(width)
          .faces("<Z")
          .shell(-th)
          # round edges
         .edges("|Z").fillet(fillet_r)
          .faces(">Z")
          .edges().fillet(fillet_r)
          )

# tag various useful workplanes

result.faces("<Z").workplane(centerOption="CenterOfMass", 
                             invert=True).tag("bottom")

top_angle = math.degrees(math.atan((mid_depth-top_depth)/bend_x))
(result
 .faces("<Z")
 .workplane(centerOption="CenterOfMass", offset=mid_depth, invert=True)
 .transformed(offset=((bend_x-height)/2, 0, -5), rotate=(0, -top_angle, 0))
 .tag("top_top")
 )

bot_angle = math.degrees(math.atan((mid_depth-bot_depth)/(height-bend_x)))
(result
 .faces("<Z")
 .workplane(centerOption="CenterOfMass", offset=mid_depth, invert=True)
 .transformed(offset=(bend_x/2, 0, -9.8), rotate=(0, bot_angle, 0))
 .tag("top_bot")
)

screwpost_d = 10.1 # must be > 2*fillet_r

def make_screwpost(o, xs, ys):
    ovec = (xs*(height - 1.2*screwpost_d)/2, ys*(width - 1.2*screwpost_d)/2, 0)
    return (o
            .workplaneFromTagged("bottom")
            .transformed(offset=ovec)
            .rect(screwpost_d, screwpost_d)
            .extrude(until='next')
            .edges("|Z")
            .fillet(2)
            .workplaneFromTagged("bottom")
            .transformed(offset=ovec)
            .circle(insert_sr+.25)
            .cutBlind(10)
            .workplaneFromTagged("bottom")
            .transformed(offset=ovec)
            .circle(insert_r)
            .cutBlind(insert_l)
            )

result = make_screwpost(result, -1, -1)
result = make_screwpost(result, -1,  1)
result = make_screwpost(result,  1, -1)
result = make_screwpost(result,  1,  1)

disp_y_offset = 3
# hole for display
result = (result
          .workplaneFromTagged("top_top")
          .transformed(offset=(disp_y_offset, 0, 0))
          .rect(disp_hole_h, disp_hole_w)
          .cutBlind(-th)
          )
# recess for display
result = (result
          .workplaneFromTagged("top_top")
          .transformed(offset=(disp_y_offset, 0, -5))
          .rect(disp_h, disp_w)
          .cutBlind(th)
          )
# screwposts for display
def make_disp_screwpost(o, xs, ys):
    ovec = (disp_y_offset+xs*disp_h_cc_y/2, ys*disp_h_cc_x/2, -4)
    return (o
            .workplaneFromTagged("top_top")
            .transformed(offset=ovec)
            .circle(1)
            .cutBlind(th)
            )

result = make_disp_screwpost(result, -1, -1)
result = make_disp_screwpost(result, -1,  1)
result = make_disp_screwpost(result,  1, -1)
result = make_disp_screwpost(result,  1,  1)

def make_button_hole(o, offset):
    ovec = (0, offset*30, -4)
    return (o
            .workplaneFromTagged("top_bot")
            .transformed(offset=ovec)
            .circle(6.5)
            .cutBlind(2*th)
            )

result = make_button_hole(result, -1)
result = make_button_hole(result, 0)
result = make_button_hole(result, 1)    

# for debugging
# result = (result
#           .workplaneFromTagged("top_top")
#           .box(50, 50, 10, centered=centerXY)
#           )
# result = (result
#          .workplaneFromTagged("bot_top")
#          .box(20, 50, 10, centered=centerXY)
#          )

show_object(result)

