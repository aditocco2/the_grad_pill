# Block 0 will be the control block to tell the program how long the video is
# Blocks 1 through N will be the frame data
# :)

# As of now bytes 0 through 3 will have a uint32_t for the length of the video in frames
# Since each frame is 8192 bytes and each sector is 512 I can just left shift by 3 B)

# Whatever, let's convert the gif

from PIL import Image, ImageSequence

width, height = 64, 64
image_file = "images/spongedance-4.gif"
sd_file = "images/onegif.img"

def main():

    gif = Image.open(image_file)

    # get number of frames and set up control block
    num_frames = gif.n_frames
    control_block = bytearray(512)
    control_block[0] = num_frames & 0xFF
    control_block[1] = (num_frames >> 8) & 0xFF
    control_block[2] = (num_frames >> 16) & 0xFF
    control_block[3] = (num_frames >> 24) & 0xFF

    # Now write the control block to the file
    # Erase anything in the file if it already exists
    with open(sd_file, "wb") as f:
        f.write(control_block)

    for frame in ImageSequence.Iterator(gif):
        frame = frame.resize((width, height))
        frame = image_to_rgb565_bytearray(frame)
        
        # Append in binary
        with open(sd_file, "ab") as f:
            f.write(frame)


def image_to_rgb565_bytearray(image):

    # Convert to RGB888 if not already RGB888
    image = image.convert("RGB")

    # Convert to RGB565 
    pixels = bytearray()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue = image.getpixel((x, y))

            red = red >> 3
            green = green >> 2
            blue = blue >> 3

            pixel = (red << 11) | (green << 5) | (blue << 0)

            low_byte = pixel & 0xFF
            high_byte = pixel >> 8

            pixels.append(low_byte)
            pixels.append(high_byte)

    return pixels

if __name__ == "__main__":
    main()