import cadquery as cq

# opi
opi_l = 46
opi_w = 48
opi_h = 32
# ethernet
eth_w = 16.5
eth_h = 13.5
eth_x_offset = 0.66
pcb_th = 1.55
# usb
usb_w = 6
usb_h = 13.5
usb_x_offset = -(19.5+2.8)
# display
disp_l = 28
disp_w = 28
disp_h = 4

centerXY = (True, True, False)

def opi(obj, print):
    pi = (obj
          .transformed(offset=(0, 0, 0))
          .box(opi_l, opi_w, opi_h, centered=centerXY))
    if print:
        return pi
    jacks = (pi
             .faces("<Y")
             .workplane()
             .transformed(offset=(-eth_w/2-eth_x_offset, pcb_th, -10))
             .box(eth_w, eth_h, 11, centered=False)
             .faces("<Y")
             .workplane()
             .transformed(offset=(-usb_w/2-usb_x_offset, 0.9, -15))
             .box(usb_w, usb_h, 15, centered=False)
             )
    display = (pi
               .faces(">Z")
               .workplane()
               .transformed(offset=(0, 35, 0))
               .box(disp_l, disp_w, disp_h, centered=centerXY)
               )
    return pi.union(jacks).union(display)

def opi_jacks_cut(obj, z_offset):
    return (obj
            .faces("<Y")
            .workplane()
            .transformed(offset=(-eth_w/2-eth_x_offset, pcb_th + z_offset, -5))
            .rect(eth_w, eth_h, centered=False)
            .cutBlind(50)
            .faces("<Y")
            .workplane()
            .transformed(offset=(-usb_w/2-usb_x_offset, 0.9, -5))
            .rect(usb_w, usb_h, centered=False)
            .cutBlind(50)
            )

