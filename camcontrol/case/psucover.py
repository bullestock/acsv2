import cadquery as cq
from defs import *

w = 34
l = 10
th = 2
h = 6

result = (cq.Workplane("XY")
          .rect(w + 2*th, l + 2*th)
          .extrude(th + h)
          .faces("<Z")
          .transformed(offset=(0, th/2, 0))
          .rect(w, l+th)
          .cutBlind(h)
          )

show_object(result)
