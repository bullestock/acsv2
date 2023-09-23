import cadquery as cq

case_th = 3
case_h = 46
case_d = 15
case_w = 160+3
coil_sup_l = 5
coil_sup_d = 3
# Inner diameter of coil
coil_sup_w = 43
coil_sup_h = 31
# Width of screw block
sw = 12
# Thickness of front plate
front_th = 4
# Screw cutout
sc_w = 10
sc_d = 5
# Screw holes
sh_dia = 4.2
sh_dist = 155 #!!

iw = case_w - 2*sw
ih = case_h - case_th

centerXY = (True, True, False)

outer = (cq.Workplane("XY")
         .tag("base")
         .box(case_w, case_h, case_d, centered=centerXY)
         .edges(">Z or |Z")
         .fillet(7.5)
         .workplaneFromTagged("base")
         .rarray(sh_dist, 1, 2, 1)
         .circle(sh_dia/2)
         .cutThruAll()
         )

inner = (cq.Workplane("XY")
         .box(iw, ih, case_d - front_th, centered=centerXY)
         .edges("|Z")
         .fillet(5)
         )

scutout1 = (cq.Workplane("XY")
           .transformed(offset=(case_w/2 - sc_w/2, 0, case_d - sc_d))
           .box(sc_w, sc_w, sc_d, centered=centerXY)
           )
scutout2 = (cq.Workplane("XY")
           .transformed(offset=(-case_w/2 + sc_w/2, 0, case_d - sc_d))
           .box(sc_w, sc_w, sc_d, centered=centerXY)
           )

res = outer-inner - scutout1 - scutout2

show_object(res)
#show_object(scutout)
