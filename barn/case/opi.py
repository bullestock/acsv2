import cadquery as cq

# opi
opi_l = 46
opi_w = 48
opi_h = 32
# ethernet
eth_w = 16.5
eth_x_offset = 0.66
pcb_th = 1.55
# display
disp_l = 28
disp_w = 28
disp_h = 4

centerXY = (True, True, False)

def opi(obj):
    return (obj
            .transformed(offset=(0, 0, 0))
            .box(opi_l, opi_w, opi_h, centered=centerXY)
            .faces(">Y")
            .workplane(origin=(0, 0, 0))
            .transformed(offset=(-eth_w/2-eth_x_offset, pcb_th, 0))
            .box(eth_w, 13.5, 5, centered=False)
            .faces(">Z")
            .workplane(origin=(0, 0, 0))
            .transformed(offset=(0, -35, 0))
            .box(disp_l, disp_w, disp_h, centered=centerXY)
            )

