import cadquery as cq
import math

# M3x4x4
insert_l = 4
insert_r = 2.1
insert_sr = 1.5

def round_standoff(d, h):
    max_d = min(h, 3*insert_l)
    return (cq.Workplane()
            .cylinder(radius=d/2, height=h)
            .faces(">Z")
            .circle(insert_r).cutBlind(-insert_l)
            .faces(">Z")
            .circle(insert_sr+.25).cutBlind(-max_d)
            )

def square_standoff(d, h, r):
    return (cq.Workplane()
            .box(d, d, h)
            .edges("|Z").fillet(r)
            .faces(">Z")
            .circle(insert_r).cutBlind(-insert_l)
            .faces(">Z")
            .circle(insert_sr+.25).cutBlind(-3*insert_l)
            )
