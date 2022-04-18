import cadquery as cq

# opi
opi_l = 48
opi_w = 46
opi_h = 22
# ethernet
eth_w = 16.5
eth_h = 13.5
eth_x_offset = 0.66
pcb_th = 1.55
# usb
usb_w = 17
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

def opi_jacks_cut(wp, x_offset, y_offset, z_offset):
    return (wp
            .transformed(offset=(x_offset, y_offset+usb_x_offset, z_offset+usb_z),
                         rotate=(90, 90, 0))
            .rarray(usb_x_cc, 1, 2, 1)
            .rect(usb_w, usb_h, centered=True)
            .cutBlind(-50)
            )
