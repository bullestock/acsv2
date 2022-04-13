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
result.faces("<Z").workplane(centerOption="CenterOfMass", 
                             invert=True).tag("bottom")
top_angle = math.degrees(math.atan((mid_depth-top_depth)/bend_x))
(result
 .faces("<Z")
 .workplane(centerOption="CenterOfMass", offset=mid_depth, invert=True)
 .transformed(offset=((bend_x-height)/2, 0, 0), rotate=(0, -top_angle, 0))
 .tag("top_top")
 )

bot_angle = math.degrees(math.atan((mid_depth-bot_depth)/(height-bend_x)))
(result
 .faces("<Z")
 .workplane(centerOption="CenterOfMass", offset=mid_depth, invert=True)
 .transformed(offset=(bend_x/2, 0, 0), rotate=(0, bot_angle, 0))
 .tag("bot_top")
)

screwpost_d = 10.1 # must be > 2*fillet_r

def make_screwpost(o, xs, ys):
    return (o
            .workplaneFromTagged("bottom")
            .transformed(offset=(xs*(height - 1.2*screwpost_d)/2, ys*(width - 1.2*screwpost_d)/2, 0))
            .rect(screwpost_d, screwpost_d)
            .extrude(until='next')
            .edges("|Z")
            .fillet(2)
            .workplaneFromTagged("bottom")
            .transformed(offset=(xs*(height - 1.2*screwpost_d)/2, ys*(width - 1.2*screwpost_d)/2, 0))
            .circle(insert_sr+.25)
            .cutBlind(10)
            .workplaneFromTagged("bottom")
            .transformed(offset=(xs*(height - 1.2*screwpost_d)/2, ys*(width - 1.2*screwpost_d)/2, 0))
            .circle(insert_r)
            .cutBlind(insert_l)
            )

result = make_screwpost(result, -1, -1)
result = make_screwpost(result, -1,  1)
result = make_screwpost(result,  1, -1)
result = make_screwpost(result,  1,  1)

# for debugging
# result = (result
#           .workplaneFromTagged("top_top")
#           .box(50, 50, 10)
#           )
# result = (result
#           .workplaneFromTagged("bot_top")
#           .box(20, 50, 10)
#           )

show_object(result)

