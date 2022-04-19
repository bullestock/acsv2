import cadquery as cq

# opi
opi_l = 48
opi_w = 46
opi_h = 22
# ethernet extender
eth_w = 20.8
eth_h = 17.5
# usb
usb_w = 18
usb_h = 10
usb_z = 18
usb_x_cc = 20
usb_x_offset = 1.4

centerXY = (True, True, False)

def opi(obj):
    pi = (obj
          .transformed(offset=(0, 0, 0))
          .box(opi_l, opi_w, opi_h, centered=centerXY))
    return pi

def opi_jacks_cut(obj, x_offset, y_offset, z_offset):
    return (obj
            .workplaneFromTagged("bottom")
            .transformed(offset=(x_offset, y_offset+usb_x_offset, z_offset+usb_z),
                         rotate=(90, 90, 0))
            .rarray(usb_x_cc, 1, 2, 1)
            .rect(usb_w, usb_h, centered=True)
            .cutBlind(-50)
            .workplaneFromTagged("bottom")
            .transformed(offset=(x_offset, y_offset+usb_x_offset, z_offset),
                         rotate=(90, 90, 0))
            .rect(eth_w, eth_h, centered=(True, False))
            .cutBlind(100)
            )

def opi_jacks_cutter(x_offset, y_offset, z_offset):
    return (cq.Workplane("XY")
            .tag("base")
            .transformed(offset=(x_offset, y_offset+usb_x_offset, z_offset+usb_z),
                         rotate=(90, 90, 0))
            .rarray(usb_x_cc, 1, 2, 1)
            .rect(usb_w, usb_h, centered=True)
            .extrude(-50)
            .edges(">Z or <Z").fillet(2)
            .workplaneFromTagged("base")
            .transformed(offset=(x_offset, y_offset+usb_x_offset, z_offset),
                         rotate=(90, 90, 0))
            .rect(eth_w, eth_h, centered=(True, False))
            .extrude(100)
            )
