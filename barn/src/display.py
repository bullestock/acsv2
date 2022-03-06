#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2014-2020 Richard Hull and contributors
# See LICENSE.rst for details.
# PYTHON_ARGCOMPLETE_OK

"""
Simple println capabilities.
"""

import time
from pathlib import Path
from luma.core.virtual import terminal
from luma.oled.device import ssd1306, sh1106
from PIL import ImageFont


def make_font(name, size):
    font_path = str(Path(__file__).resolve().parent.joinpath('fonts', name))
    return ImageFont.truetype(font_path, size)

class Display:
    def __init__(self):
        font = make_font("ProggyTiny.ttf", 16)
        device = sh1106()
        self.term = terminal(device, font, animate=False)

    def println(self, txt):
        self.term.println(txt)
        
    def demo(self):
        self.term.println("Terminal mode demo")
        self.term.println("------------------")
        self.term.println("Uses any font to output text using a number of different print methods.")
        self.term.println()
        time.sleep(2)
        self.term.println("This font supports a terminal size of {0}x{1} characters.".format(self.term.width, self.term.height))
        self.term.println()
        time.sleep(2)


if __name__ == "__main__":
    try:
        d = Display()
        d.demo()
    except KeyboardInterrupt:
        pass
