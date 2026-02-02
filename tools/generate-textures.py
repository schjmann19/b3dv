#!/usr/bin/env python3
# Copyright (c) 2026 Jimena Neumann
# SPDX-License-Identifier: BSD-3-Clause

from PIL import Image
import random
import os

SIZE = 100
OUT_DIR = "assets/textures/blocks"

os.makedirs(OUT_DIR, exist_ok=True)

# ---- block definitions ----
# name : (base_color, variation)
BLOCKS = {
    "dirt":    ((120, 100, 80),  25),
    "grass":   ((95, 140, 70),   20),
    "stone":   ((125, 125, 125), 18),
    "sand":    ((200, 190, 140), 15),
    "wood":    ((140, 110, 70),  22),
    "bedrock": ((21, 21, 21),    10),
}

def clamp(v):
    return max(0, min(255, v))

def generate_block(name, base_color, variation):
    img = Image.new("RGB", (SIZE, SIZE))
    pixels = img.load()

    for y in range(SIZE):
        for x in range(SIZE):
            r = base_color[0] + random.randint(-variation, variation)
            g = base_color[1] + random.randint(-variation, variation)
            b = base_color[2] + random.randint(-variation, variation)

            # occasional bright spot
            if random.random() < 0.08:
                r += 15
                g += 15
                b += 15

            # occasional darker spot
            if random.random() < 0.05:
                r -= 20
                g -= 20
                b -= 20

            pixels[x, y] = (clamp(r), clamp(g), clamp(b))

    path = f"{OUT_DIR}/{name}.png"
    img.save(path)
    print(f"Generated {path}")

# ---- generate everything ----
for name, (color, var) in BLOCKS.items():
    generate_block(name, color, var)
