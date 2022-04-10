import cadquery as cq

# M3x4x4
insert_l = 4
insert_r = 2.1

def standoff(d, h):
    return (cq.Workplane()
            .cylinder(radius=d/2, height=h)
            .faces(">Z")
            .circle(insert_r).cutBlind(-insert_l)
            )
