import cadquery as cq
import cq_warehouse.extensions
import orangepizero as opz
import standoff

print = True
#print = False

height = 90
width = 75
thickness = 35
opi_y_offset = -17

holes_dx = 40.1109
holes_dy = 42.11087

# shell thickness
th = 3
# fillet radius
fillet_r = 5

# standoff dimensions
standoff_h = 5
standoff_d = 7

# define opi standoff
pi_standoff = standoff.standoff(standoff_d, standoff_h)

centerXY = (True, True, False)

# make shell
shell = (cq.Workplane("XY")
         .box(width, height, thickness, centered=centerXY)
         .faces(">Z")
         .shell(-th)
         # round edges
         .edges("<Z or |Z").fillet(fillet_r)
        )

# distribute opi standoffs
opi_standoffs = (shell
                 .faces("<Z")
                 # place workplane on bottom of box, with correct Z orientation
                 .workplane(origin=(0, 0, th), invert=True)
                 # place standoffs on bottom
                 .transformed(offset=(0, opi_y_offset, standoff_h))
                 .rect(holes_dx, holes_dy, forConstruction=True)
                 .vertices()
                 .eachpoint(lambda loc: pi_standoff.val().moved(loc), True)
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
    shell = opz.opi_jacks_cut(shell, th+standoff_h)

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
result = shell.union(opi_standoffs).union(smps)
if not print:
    result = result.union(opi)

show_object(result)
#show_object(opi)

# TODO:
# inserts for assembly
# screw holes for wall fitting
# power plug
# 12 V out for lock

