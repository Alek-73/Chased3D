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


def bitmap(x, y, rows, scale, color):
    for row, bits in enumerate(rows):
        for column in range(8):
            if bits & (0x80 >> column):
                rectangle(x + column * scale, y + row * scale,
                          scale, scale, color)


# Pixel codes 0 and 1 are the only colors used. The frame shows the maze from
# the player's viewpoint, with the decoy between the viewer and the pursuer.
# Main corridor: three sparse wall outlines establish depth.
for x0, y0, x1, y1 in (
        (0, 2, 52, 13), (0, 44, 52, 32),
        (159, 2, 108, 13), (159, 44, 108, 32),
        (52, 13, 52, 32), (108, 13, 108, 32),
        (52, 13, 108, 13), (52, 32, 108, 32),
        (65, 17, 95, 17), (65, 28, 95, 28),
        (65, 17, 65, 28), (95, 17, 95, 28),
        (73, 20, 87, 20), (73, 26, 87, 26),
        (73, 20, 73, 26), (87, 20, 87, 26)):
    line(x0, y0, x1, y1, 1)

# Keep each sprite's local background dark so the wall outlines do not cut
# through it, while preserving the corridor between the three objects.
rectangle(47, 10, 18, 34, 0)
rectangle(74, 16, 12, 20, 0)
rectangle(95, 10, 18, 34, 0)

# Exact in-game decoy sprite, enlarged in the left foreground.
decoy_sprite = (
    0x18, 0x24, 0x42, 0x81, 0x18, 0x24, 0x42, 0x00,
    0x18, 0x24, 0x42, 0x18, 0x24, 0x81, 0xFF, 0xFF,
)
bitmap(48, 11, decoy_sprite, 2, 1)

# Exact in-game 8x16 laser glyph, centered between the two objects.
laser_sprite = (
    0x00, 0x81, 0x24, 0x5A, 0x18, 0x7E, 0x3C, 0xFF,
    0xFF, 0x3C, 0x7E, 0x18, 0x5A, 0x24, 0x81, 0x00,
)
bitmap(76, 18, laser_sprite, 1, 1)

# Exact in-game collectible target glyph, enlarged in the right foreground.
target_sprite = (
    0xE1, 0x92, 0x4C, 0x4A, 0x31, 0x31, 0x4D, 0x83,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF, 0xFF,
)
bitmap(96, 11, target_sprite, 2, 1)

palette = (
    (12, 8, 4, 0),
    (220, 238, 248, 0),
    (12, 8, 4, 0),
    (220, 238, 248, 0),
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

print(f"Generated two-color cartoon splash: {output_path}")
