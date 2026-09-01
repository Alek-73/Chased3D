import struct
from pathlib import Path


WIDTH = 160
HEIGHT = 46
pixels = [[0 for _ in range(WIDTH)] for _ in range(HEIGHT)]


def pixel(x, y, color):
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        pixels[y][x] = color


def line(x0, y0, x1, y1, color):
    dx = abs(x1 - x0)
    step_x = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    step_y = 1 if y0 < y1 else -1
    error = dx + dy
    while True:
        pixel(x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        twice_error = 2 * error
        if twice_error >= dy:
            error += dy
            x0 += step_x
        if twice_error <= dx:
            error += dx
            y0 += step_y


def rectangle(x, y, width, height, color):
    for row in range(y, y + height):
        for column in range(x, x + width):
            pixel(column, row, color)


def ellipse(center_x, center_y, radius_x, radius_y, color):
    limit = radius_x * radius_x * radius_y * radius_y
    for y in range(center_y - radius_y, center_y + radius_y + 1):
        for x in range(center_x - radius_x, center_x + radius_x + 1):
            dx = x - center_x
            dy = y - center_y
            if dx * dx * radius_y * radius_y + dy * dy * radius_x * radius_x <= limit:
                pixel(x, y, color)


tiny_font = {
    "E": (0b111, 0b100, 0b110, 0b100, 0b111),
    "K": (0b101, 0b101, 0b110, 0b101, 0b101),
    "!": (0b010, 0b010, 0b010, 0b000, 0b010),
}


def tiny_text(x, y, text, color, scale=1):
    for character in text:
        for row, bits in enumerate(tiny_font[character]):
            for column in range(3):
                if bits & (0b100 >> column):
                    rectangle(x + column * scale, y + row * scale,
                              scale, scale, color)
        x += 4 * scale


# A clean comic panel: one joke, two silhouettes, and enough empty space to
# remain readable on a real 160-pixel Atari display.
rectangle(1, 1, WIDTH - 2, 2, 1)
rectangle(1, HEIGHT - 3, WIDTH - 2, 2, 1)
rectangle(1, 1, 2, HEIGHT - 2, 1)
rectangle(WIDTH - 3, 1, 2, HEIGHT - 2, 1)

# Restrained motion lines point toward the chase without filling the panel.
for x0, y, x1 in ((7, 8, 22), (4, 14, 17), (6, 37, 22),
                  (58, 7, 82), (61, 13, 75), (56, 39, 82),
                  (119, 27, 151), (124, 34, 153), (127, 40, 146)):
    line(x0, y, x1, y, 1)

# Round pursuer with four bent limbs, inspired by the original orb enemy.
ellipse(37, 23, 19, 16, 2)
for points in (
        ((23, 12), (14, 6), (17, 3)),
        ((50, 11), (58, 5), (56, 2)),
        ((22, 34), (13, 39), (16, 43)),
        ((51, 33), (61, 38), (58, 43))):
    line(*points[0], *points[1], 2)
    line(*points[1], *points[2], 2)
    line(points[0][0] + 1, points[0][1],
         points[1][0] + 1, points[1][1], 2)

# Angry eyes and broad mouth remain readable at native resolution.
ellipse(30, 19, 5, 5, 1)
ellipse(44, 19, 5, 5, 1)
line(26, 16, 34, 19, 0)
line(48, 16, 40, 19, 0)
rectangle(30, 20, 2, 3, 0)
rectangle(42, 20, 2, 3, 0)
rectangle(27, 28, 20, 5, 0)
line(29, 28, 45, 28, 1)
for tooth_x in range(30, 45, 4):
    line(tooth_x, 29, tooth_x + 2, 32, 1)

# Ugug is one tapered triangular body, with a soft point and running limbs.
for y in range(13, 38):
    half_width = (y - 10) // 2
    rectangle(103 - half_width, y, half_width * 2 + 1, 1, 1)
ellipse(103, 14, 3, 3, 1)

# Friendly face, waving arm, trailing arm, and exaggerated running feet.
ellipse(99, 23, 3, 4, 2)
ellipse(107, 23, 3, 4, 2)
rectangle(100, 22, 1, 2, 0)
rectangle(106, 22, 1, 2, 0)
rectangle(101, 29, 6, 3, 2)
line(102, 30, 106, 30, 0)
line(92, 29, 83, 25, 1)
line(114, 28, 122, 21, 1)
line(122, 21, 121, 17, 1)
line(122, 21, 126, 19, 1)
line(99, 37, 90, 43, 1)
line(108, 37, 119, 42, 1)
line(90, 43, 84, 43, 1)
line(119, 42, 125, 42, 1)

# Compact speech bubble, separated from both characters.
rectangle(119, 5, 34, 13, 1)
rectangle(117, 7, 38, 9, 1)
rectangle(121, 7, 30, 9, 0)
rectangle(119, 9, 34, 5, 0)
line(122, 16, 112, 22, 1)
line(123, 16, 118, 21, 1)
tiny_text(128, 9, "EEK!", 2)

# Dust puffs and a simple ground line finish the chase without visual noise.
line(68, 43, 131, 43, 2)
for puff_x, puff_y in ((84, 39), (79, 37), (75, 40)):
    ellipse(puff_x, puff_y, 2, 1, 1)

palette = (
    (8, 8, 12, 0),
    (18, 104, 132, 0),
    (212, 70, 184, 0),
    (0, 0, 0, 0),
)
row_bytes = WIDTH // 2
data_size = row_bytes * HEIGHT
pixel_offset = 14 + 40 + len(palette) * 4
file_size = pixel_offset + data_size

with Path(__file__).with_name("SPLASH.BMP").open("wb") as output:
    output.write(struct.pack("<2sIHHI", b"BM", file_size, 0, 0, pixel_offset))
    output.write(struct.pack("<IIIHHIIIIII", 40, WIDTH, HEIGHT, 1, 4, 0,
                             data_size, 0, 0, len(palette), len(palette)))
    for entry in palette:
        output.write(bytes(entry))
    for row in reversed(pixels):
        for x in range(0, WIDTH, 2):
            output.write(bytes([(row[x] << 4) | row[x + 1]]))