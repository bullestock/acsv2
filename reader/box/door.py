#! /usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import division
import os
import sys
import re

# Assumes SolidPython is in site-packages or elsewhwere in sys.path
from solid import *
from solid.utils import *

SEGMENTS = 16

case_th = 1.5
case_h = 42
case_d = 18
case_w = 160
coil_sup_l = 3
coil_sup_d = 3
# Inner diameter of coil
coil_sup_w = 43
coil_sup_h = 31
# Width of screw block
sw = 12
# Thickness of front plate
front_th = 1

def coil_sup():
    return cylinder(coil_sup_d/2, coil_sup_l)

def coil_sups():
    s1 = translate([-(coil_sup_w-coil_sup_d)/2, -(coil_sup_h-coil_sup_d)/2, 0])(coil_sup())
    s2 = translate([ (coil_sup_w-coil_sup_d)/2, -(coil_sup_h-coil_sup_d)/2, 0])(coil_sup())
    s3 = translate([ (coil_sup_w-coil_sup_d)/2,  (coil_sup_h-coil_sup_d)/2, 0])(coil_sup())
    s4 = translate([-(coil_sup_w-coil_sup_d)/2,  (coil_sup_h-coil_sup_d)/2, 0])(coil_sup())
    return s1+s2+s3+s4

def bottom():
    # Actually, this is the front
    return color(Red)(translate([-case_w/2, -case_h/2, 0])(cube([case_w, case_h, front_th])))

def frame():
    outer = translate([-case_w/2, -case_h/2, front_th])(cube([case_w, case_h, case_d]))
    iw = case_w-2*case_th
    ih = case_h-2*case_th
    inner = translate([-iw/2, -ih/2, front_th])(cube([iw, ih, case_d+5]))
    return outer-inner

def led_support():
    return cylinder(h=4, d=8)

def led_hole():
    return down(1)(cylinder(h=10, d=5.1))

def screw_block(left):
    block = cube([sw, case_h, case_d+front_th])
    offset = case_th/2
    if not left:
        offset = -offset
    hole = translate([sw/2+offset, case_h/2, 0])(cylinder(h=case_h+2, r=2) +
                                            down(0.1)(cylinder(h=4, r1=4.5, r2=2)))
    return color(Green)(translate([-sw/2, -case_h/2, 0])(block - hole))

def assembly():
    bt = bottom()
    cs = up(case_th)(right(case_w/2 - coil_sup_w/2 - 5)(coil_sups()))
    fr = frame()
    led_cc = 17.17
    led_from_bot = 5
    led_tr = case_w/2 - led_from_bot
    l1 = forward(led_cc/2)(led_support())
    lh1 = forward(led_cc/2)(led_hole())
    l2 = back(led_cc/2)(led_support())
    lh2 = back(led_cc/2)(led_hole())
    s1 = left(case_w/2+sw/2-0.1)(screw_block(True))
    s2 = right(case_w/2+sw/2-0.1)(screw_block(False))
    return cs+fr+bt+left(led_tr)(l1+l2)+s1+s2-left(led_tr)(lh1+lh2) - translate([-35, 0, 1])(cylinder(h = 20, d = 5))

if __name__ == '__main__':
    a = assembly()
    scad_render_to_file(a, file_header='$fn = %s;' % SEGMENTS, include_orig_code=False)

# Local Variables:
# compile-command: "python door.py"
# End:
