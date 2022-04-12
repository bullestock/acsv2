import cadquery as cq
from defs import *

h = 15
w = wh_dist + h
# thickness of screw-on part
th1 = 3
# thickness of trapezoidal part
th2 = 5
# minimum width of trapezoidal part
min_w = 30
slope = 0.7

result = (cq.Workplane("XY")
          .hLine(-w/2) # bottom
          .vLine(th1)  # edge
          .hLine(w/2 - min_w/2) # flat upper part
          .lineTo(-(min_w/2 + slope*th2), th1+th2)
          .lineTo(0, th1+th2)
          .mirrorY()
          .extrude(h)
          .edges("|Z")
          .fillet(0.5)
          )

result = (result
          .faces(">Z")
          .transformed(offset=(0, th1, h/2), rotate=(90, 180, 0))
          .rarray(wh_dist, 1, 2, 1)
          .cskHole(3.5, 6, 82)
          )

show_object(result)
