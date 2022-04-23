import cadquery as cq

centerXY = (True, True, False)
fillet_r = 1

usb_w = 18
usb_h = 10
usb_z = 18
usb_x_cc = 20

result = (cq.Workplane("XY")
          .box(52, 14, 9.5, centered=centerXY)
          # round edges
          .edges("|Z").fillet(fillet_r)
          .rarray(usb_x_cc + usb_w + 8, 1, 2, 1)
          .circle(1.25)
          .cutThruAll()
         )

cutter = (cq.Workplane("XY")
          .rarray(usb_x_cc, 1, 2, 1)
          .rect(usb_w, usb_h, centered=True)
          .extrude(20)
          # round
          .edges("|Z").fillet(2)
          )

result = result - cutter

show_object(result)
