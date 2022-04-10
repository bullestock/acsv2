import cadquery as cq
import cq_warehouse.extensions
import orangepizero as opz

print = True
#print = False

height = 90
width = 75
thickness = 35
opi_y_offset = -17

holes_dx = 40.1109
holes_dy = 42.11087

# M3x4x4
insert_l = 4
insert_r = 2.1

# shell thickness
th = 3
# fillet radius
fillet_r = 5

# standoff dimensions
standoff_h = 5
standoff_d = 7

# define standoff
standoff = (cq.Workplane()
            .cylinder(radius=standoff_d/2, height=standoff_h)
            .faces(">Z")
            .circle(insert_r).cutBlind(-insert_l)
            )

centerXY = (True, True, False)

# make shell
shell = (cq.Workplane("XY")
         .box(width, height, thickness, centered=centerXY)
         .faces(">Z")
         .shell(-th)
         # round edges
         .edges("<Z or |Z").fillet(fillet_r)
        )

# distribute standoffs
standoffs = (shell
             .faces("<Z")
             # place workplane on bottom of box, with correct Z orientation
             .workplane(origin=(0, 0, th), invert=True)
             # place standoffs on bottom
             .transformed(offset=(0, opi_y_offset, standoff_h))
             .rect(holes_dx, holes_dy, forConstruction=True)
             .vertices()
             .eachpoint(lambda loc: standoff.val().moved(loc), True)
            )

# opi
opi = opz.opi(shell
              .faces("<Z")
              .workplane(origin=(0, 0, th), invert=True)
              # why is '+th' needed here?
              .transformed(offset=(0,
                                   opi_y_offset,
                                   th+standoff_h)),
              print)

if print:
    shell = opz.opi_eth_cut(shell, th+standoff_h)

# smps
smps_l = 30
smps_w = 18.5
smps_h = 7.5
smps_wall_h = 4
smps_wall_th = 2.5
smps_y_offset = 25

if print:
    ex = 2*smps_wall_th
    smps = (cq.Workplane()
            .transformed(offset=(0, smps_y_offset, th))
            .box(smps_l+ex, smps_w+ex, smps_wall_h, centered=centerXY)
            .faces("|Z")
            .shell(-smps_wall_th)
            )
else:
    smps = (cq.Workplane()
            .transformed(offset=(0, smps_y_offset, th))
            .box(smps_l, smps_w, smps_h, centered=centerXY)
            )

# combine
result = shell.union(standoffs).union(smps)
if not print:
    result = result.union(opi)

show_object(result)
#show_object(opi)

# TODO:
# screw holes for wall fitting
# cutouts for USB
# cutout + round hole for ethernet cable
# stepdown
# power plug
# 12 V out for lock

