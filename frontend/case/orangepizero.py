import cadquery as cq

# opi
opi_l = 48
opi_w = 46
opi_h = 32 #!!
# ethernet
eth_w = 16.5
eth_h = 13.5
eth_x_offset = 0.66
pcb_th = 1.55
# usb
usb_w = 6.5
usb_h = 14
usb_x_offset = -(19.5+2.8)+0.5

centerXY = (True, True, False)

def opi(obj):
    pi = (obj
          .transformed(offset=(0, 0, 0))
          .box(opi_l, opi_w, opi_h, centered=centerXY))
    return pi
