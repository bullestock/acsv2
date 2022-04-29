import cadquery as cq

mh_cc = 50
total_h = 31+2.6
foot_hole_d = 3.5
disp_hole_d = 2.5
dhx_cc = 30.48
dhy_cc = 28.448
width = 12
th = 3
# display is shifted this much to the right
offset = -0.625

result = (cq.Workplane("XY")
          .tag("base")
          # bar
          .box(mh_cc+width, width, th, centered=(True, True, False))
          # holes for case screws
          .transformed(offset=(offset, 0, 0))
          .rarray(mh_cc, 1, 2, 1)
          .slot2D(foot_hole_d*2, foot_hole_d, 90)
          .cutThruAll()
         )
offset = 2

result = (result
          # display mount plate
          .workplaneFromTagged("base")
          .box(36, 36, th, centered=(True, True, False))
          .edges("|Z")
          .fillet(1)
          # supports for display
          .workplaneFromTagged("base")
          .transformed(offset=(0, 0, th))
          .rect(dhx_cc, dhx_cc, forConstruction=True)
          .vertices()
          .circle(disp_hole_d)
          .extrude(2)
          # holes for display screws
          .rect(dhx_cc, dhx_cc, forConstruction=True)
          .vertices()
          .circle(disp_hole_d/2)
          .cutThruAll()
          # cutout for connector
          .workplaneFromTagged("base")
          .transformed(offset=(0, 15, 0))
          .rect(15, 15)
          .cutThruAll()
          )

show_object(result)
