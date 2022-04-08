import cadquery as cq
import cq_warehouse.extensions
from cq_warehouse.fastener import HeatSetNut

#!! TODO
height = 100
width = 120
thickness = 25

holes_dx = 40.1109
holes_dy = 42.11087

#!! fix
nut = HeatSetNut(size="M3-0.5-Standard", fastener_type="McMaster-Carr")

# shell thickness
th = 3
# fillet radius
fillet_r=thickness/2-2

# standoff dimensions
standoff_h = 5
standoff_d = 7

result = (cq.Workplane("XY")
          .box(width, height, thickness)
          .faces(">Z")
          .shell(-th)
          .faces("<Z")
          .workplane(origin=(0, 0, 0)) # place OPi
          # place standoffs on bottom
          .transformed(offset=(0, 0, -th))
          .rect(holes_dx, holes_dy, forConstruction=True)
          .vertices()
          .tag("screwholes")
          .cylinder(radius=standoff_d/2, height=standoff_h)
          # add insert holes to standoffs
          .vertices(tag="screwholes")
          .insertHole(fastener=nut)
          # round edges
          .edges("<Z or |Z").fillet(fillet_r)
          )

