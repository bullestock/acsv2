import cadquery as cq

mh_cc = 50
total_h = 31+2.6
foot_hole_d = 3.5
disp_hole_d = 2.5
dhx_cc = 0.91*25.4
dhy_cc = 0.93*25.4
width = 10
th = 3

foot = (cq.Workplane("XY")
       .box(width, width, th, centered=(True, True, False))
       .circle(foot_hole_d/2)
       .cutThruAll()
       )

result = (cq.Workplane("XY")
          .tag("base")
          .rarray(mh_cc, 1, 2, 1)
          .eachpoint(lambda loc: foot.val().moved(loc), True)
         )
bw = 40
offset = 2

result = (result
          .rarray(bw, 1, 2, 1)
          .rect(th, width)
          .extrude(total_h-offset)
          )

result = (result
          .workplaneFromTagged("base")
          .transformed(offset=(0, 0, total_h-th-offset))
          .box(bw, width, th, centered=(True, True, False))
          )

result = (result
          .workplaneFromTagged("base")
          .transformed(offset=(0, 0, total_h-th-offset))
          .box(30, 30, th, centered=(True, True, False))
          .edges("|Z")
          .fillet(1)
          .workplaneFromTagged("base")
          .transformed(offset=(0, 0, total_h-th+offset/2))
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
