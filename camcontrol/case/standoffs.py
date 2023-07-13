import cadquery as cq
import math
from defs import *

# Standoff for PCB
def round_standoff(insert_r, insert_sr, insert_l, d, h):
    max_d = min(h, 3*insert_l)
    return (cq.Workplane()
            .cylinder(radius=d/2, height=h)
            .faces(">Z")
            .circle(insert_r).cutBlind(-insert_l)
            .faces(">Z")
            .circle(insert_sr+.25).cutBlind(-max_d)
            )

# Screwpost for corners of a box, with heat insert nut
def square_screwpost_nut(d, h, r):
    return (cq.Workplane()
            .box(d, d, h)
            .edges("|Z").fillet(r)
            .faces(">Z")
            # hole for insert
            .circle(insert_r).cutBlind(-insert_l)
            # hole for screw end
            .faces(">Z")
            .circle(insert_sr+.25).cutBlind(-3*insert_l)
            )

# Screwpost for corners of a box
def square_screwpost_body(d, h, r):
    return (cq.Workplane()
            .box(d, d, h)
            .edges("|Z").fillet(r)
            )
