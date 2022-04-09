import cadquery as cq
import cq_warehouse.extensions

#!! TODO
height = 100
width = 120
thickness = 25

holes_dx = 40.1109
holes_dy = 42.11087

# M3x4x4
insert_l = 4
insert_r = 2.1

# shell thickness
th = 3
# fillet radius
fillet_r=thickness/2-2

# standoff dimensions
standoff_h = 5
standoff_d = 7

# define standoff
standoff = (cq.Workplane()
            .cylinder(radius=standoff_d/2, height=standoff_h)
            .faces(">Z")
            .circle(insert_r).cutBlind(-insert_l)
            )

# make shell
shell = (cq.Workplane()
          .box(width, height, thickness)
          .faces(">Z")
          .shell(-th)
          # round edges
          .edges("<Z or |Z").fillet(fillet_r)
          )

# distribute standoffs
standoffs = (shell
          .workplane(origin=(0, 0, 0)) # place OPi
          # place standoffs on bottom
          .transformed(offset=(0, 0, -th))
          .rect(holes_dx, holes_dy, forConstruction=True)
          .vertices()
          .eachpoint(lambda loc: standoff.val().moved(loc), True)
          )

# combine
result = shell.union(standoffs)

show_object(result)

# TODO:
# screw holes for wall fitting
# cutouts for USB
# cutout + round hole for ethernet cable
# stepdown
# power plug
# 12 V out for lock

