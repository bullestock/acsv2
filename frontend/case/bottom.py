import cadquery as cq
import cq_warehouse.extensions
from cq_warehouse.fastener import HeatSetNut

height = 100
width = 120
thickness = 25

holes_dx = 40.1109
holes_dy = 42.11087

nut = HeatSetNut(size="M3-0.5-Standard", fastener_type="McMaster-Carr")

th = 3
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
          )

# dist  42.11087, 40.1109
