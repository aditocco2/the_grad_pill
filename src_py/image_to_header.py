import os
from PIL import Image

input_file = "images/sd_error.png"

width = 64
height = 64

output_file = os.path.splitext(input_file)[0] + ".h"
identifier = os.path.splitext(os.path.basename(input_file))[0]

image = Image.open(input_file)

# Convert to RGB888 if not already RGB888
image = image.convert("RGB")

# Convert to RGB565 
pixels_16b = []
for y in range(height):
    for x in range(height):
        red, green, blue = image.getpixel((x, y))

        red = red >> 3
        green = green >> 2
        blue = blue >> 3

        pixel_16b = (red << 11) | (green << 5) | (blue << 0)

        pixels_16b.append(pixel_16b)

# Print RGB565 pixels to header file
text = "static uint16_t __attribute__((aligned(4))) " \
+ identifier + "[] = {"

for i, pixel in enumerate(pixels_16b):

    if i % 8 == 0:
        text += "\n\t"

    text += f"0x{(pixel):0{4}x}, "

text += "\n\n};"

with open(output_file, "w") as f:
    f.write(text) 

print(f"Converted {input_file} to {output_file}")

