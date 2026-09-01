import struct
from pathlib import Path


WIDTH = 160
HEIGHT = 46
pixels = [[0 for _ in range(WIDTH)] for _ in range(HEIGHT)]


def pixel(x, y, color):
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        pixels[y][x] = color


def rectangle(x, y, width, height, color):
    for row in range(y, y + height):
        for column in range(x, x + width):
            pixel(column, row, color)


def line(x0, y0, x1, y1, color):
    dx = abs(x1 - x0)
    step_x = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    step_y = 1 if y0 < y1 else -1
    error = dx + dy
    while True:
        pixel(x0, y0, color)
        if x0 == x1 and y0 == y1:
            return
        twice_error = error * 2
        if twice_error >= dy:
            error += dy
            x0 += step_x
        if twice_error <= dx:
            error += dx
            y0 += step_y


def ellipse(center_x, center_y, radius_x, radius_y, color):
    limit = radius_x * radius_x * radius_y * radius_y
    for y in range(center_y - radius_y, center_y + radius_y + 1):
        for x in range(center_x - radius_x, center_x + radius_x + 1):
            dx = x - center_x
            dy = y - center_y
            value = dx * dx * radius_y * radius_y + dy * dy * radius_x * radius_x
            if value <= limit:
                pixel(x, y, color)


# Pixel codes: 0 black, 1 red maze/pursuer, 2 pink Ugug/target/highlights.
# Code 3 is reserved exclusively for the CHASED 3D title at runtime.
# The composition follows the supplied artwork but is redrawn at native size.
rectangle(0, 0, WIDTH, 2, 1)
rectangle(0, HEIGHT - 2, WIDTH, 2, 1)
rectangle(0, 0, 2, HEIGHT, 1)
rectangle(WIDTH - 2, 0, 2, HEIGHT, 1)

# Dungeon architecture with large black openings and sparse brick joints.
rectangle(8, 2, 11, 42, 1)
rectangle(12, 2, 4, 42, 0)
rectangle(55, 2, 58, 12, 1)
rectangle(59, 5, 54, 7, 0)
rectangle(18, 39, 112, 5, 1)
rectangle(23, 39, 45, 2, 0)
rectangle(88, 39, 38, 2, 0)
rectangle(126, 2, 30, 42, 1)
rectangle(129, 4, 24, 37, 0)
for x0, y, x1 in (
        (56, 5, 67), (72, 5, 84), (89, 5, 101),
        (61, 9, 76), (81, 9, 94), (99, 9, 111),
        (20, 42, 42), (49, 42, 69), (94, 42, 116),
        (130, 7, 140), (143, 7, 152), (131, 36, 145)):
    line(x0, y, x1, y, 0)

# Pink Ugug: one triangular silhouette, worried face, and running pose.
for y in range(13, 35):
    half_width = (y - 9) // 2
    rectangle(37 - half_width, y, half_width * 2 + 1, 1, 2)
ellipse(37, 14, 3, 3, 2)
rectangle(32, 21, 3, 3, 2)
rectangle(39, 21, 3, 3, 2)
pixel(33, 22, 0)
pixel(40, 22, 0)
line(34, 28, 40, 28, 0)
line(28, 27, 22, 23, 2)
line(46, 27, 52, 31, 2)
line(33, 34, 27, 40, 2)
line(41, 34, 49, 39, 2)
line(27, 40, 23, 39, 2)
line(49, 39, 54, 39, 2)
for x, y in ((26, 17), (23, 20), (28, 12)):
    line(x, y, x - 2, y - 2, 2)

# Red pursuer: round body, mechanical eye, and four rotor pods.
ellipse(89, 24, 15, 13, 1)
ellipse(89, 24, 10, 9, 0)
ellipse(89, 24, 7, 7, 1)
ellipse(89, 24, 4, 4, 2)
ellipse(89, 24, 2, 2, 0)
pixel(88, 23, 2)
for x0, y0, x1, y1 in (
        (76, 17, 67, 11), (102, 17, 111, 11),
        (76, 31, 66, 36), (102, 31, 112, 36)):
    line(x0, y0, x1, y1, 1)
    line(x0, y0 + 1, x1, y1 + 1, 1)
for rotor_x, rotor_y in ((62, 9), (116, 9), (61, 38), (117, 38)):
    ellipse(rotor_x, rotor_y, 9, 3, 1)
    ellipse(rotor_x, rotor_y, 6, 1, 0)
    line(rotor_x - 3, rotor_y, rotor_x + 3, rotor_y, 2)

# Restrained comic speed lines.
for x0, y, x1 in (
        (18, 10, 35), (16, 12, 31), (50, 20, 67),
        (48, 23, 64), (106, 5, 122), (109, 7, 125)):
    line(x0, y, x1, y, 2)

# Locked door and a bright collectible target beside it.
rectangle(133, 10, 17, 29, 1)
rectangle(135, 12, 13, 25, 0)
rectangle(137, 14, 9, 8, 1)
rectangle(137, 25, 9, 10, 1)
ellipse(141, 27, 2, 2, 2)
line(141, 29, 141, 32, 2)
rectangle(119, 25, 7, 9, 2)
rectangle(116, 27, 13, 5, 2)
rectangle(121, 23, 3, 13, 2)
pixel(120, 28, 0)
pixel(125, 28, 0)
line(120, 32, 125, 32, 0)

palette = (
    (8, 8, 12, 0),       # black
    (32, 40, 204, 0),    # red
    (212, 70, 184, 0),   # pink
    (40, 220, 240, 0),   # yellow
)
row_bytes = WIDTH // 2
pixel_offset = 14 + 40 + len(palette) * 4
data_size = row_bytes * HEIGHT
output_path = Path(__file__).with_name("SPLASH.BMP")

with output_path.open("wb") as output:
    output.write(struct.pack("<2sIHHI", b"BM", pixel_offset + data_size, 0, 0,
                             pixel_offset))
    output.write(struct.pack("<IIIHHIIIIII", 40, WIDTH, HEIGHT, 1, 4, 0,
                             data_size, 0, 0, len(palette), len(palette)))
    for entry in palette:
        output.write(bytes(entry))
    for row in reversed(pixels):
        for x in range(0, WIDTH, 2):
            output.write(bytes([(row[x] << 4) | row[x + 1]]))
