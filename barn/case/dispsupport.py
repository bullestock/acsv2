import cadquery as cq

mh_cc = 50
total_h = 31+2.6
foot_hole_d = 3.5
disp_hole_d = 2.5
dhx_cc = 0.91*25.4
dhy_cc = 0.93*25.4
width = 12
th = 3

result = (cq.Workplane("XY")
          .tag("base")
          .box(mh_cc+width, width, th, centered=(True, True, False))
          .rarray(mh_cc, 1, 2, 1)
          .slot2D(foot_hole_d*2, foot_hole_d, 90)
          .cutThruAll()
         )
offset = 2

result = (result
          .workplaneFromTagged("base")
          .box(30, 30, th, centered=(True, True, False))
          .edges("|Z")
          .fillet(1)
          .workplaneFromTagged("base")
          .transformed(offset=(0, 0, th))
          .rect(dhx_cc , dhx_cc, forConstruction=True)
          .vertices()
          .circle(disp_hole_d)
          .extrude(2)
          .rect(dhx_cc , dhx_cc, forConstruction=True)
          .vertices()
          .circle(disp_hole_d/2)
          .cutThruAll()
          .workplaneFromTagged("base")
          .transformed(offset=(0, 15, 0))
          .rect(15, 15)
          .cutThruAll()
          )

show_object(result)
