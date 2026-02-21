import os
from PIL import Image, ImageSequence

width, height = 64, 64
gif_dir = f"{os.path.expanduser('~')}/pictures/funny_gifs"
sd_file = "raw/pm2.bin"

image_files = [f"{gif_dir}/{f}" for f in os.listdir(gif_dir)]

def main():

    sector_num = 0
    f = open(sd_file, "wb")

    num_files = len(image_files)

    # first 16 bytes store metadata about the whole thing
    # each 16 bytes after that will store data about each file
    table_bytes = 16 + 16 * num_files
    num_table_sectors = table_bytes // 512 + 1

    # write empty table to sd file, we will overwrite it later
    table = bytearray(num_table_sectors * 512)
    f.write(table)

    # table header byte assignment
    # 0 through 3: number of files
    # 4 through 15: unused (TBD mode select)
    table[0:4] = to_little_endian(num_files, 4)
    table_byte_index = 16

    sector_num += num_table_sectors

    for i, file in enumerate(image_files):

        print(f"processing {file} (file {i+1}/{num_files})")

        image = Image.open(file)
        if file.lower().endswith("gif"):
            # convert and write pixel data
            data = gif_to_bytearray(image)
            f.write(data)

            # update metadata and write row to table
            n_frames = image.n_frames
            frame_duration_ms = image.info["duration"]

            # table row byte assignment:
            # 0 through 3: sector address
            # 4 through 7: number of frames
            # 8 through 9: frame duration
            # 10 through 15: unused
            tb = table_byte_index
            table[tb  :tb+4 ] = to_little_endian(sector_num, 4)
            table[tb+4:tb+8 ] = to_little_endian(n_frames, 4)
            table[tb+8:tb+10] = to_little_endian(frame_duration_ms, 2)
            table_byte_index += 16

            # update sector num for next pass
            sector_num += (n_frames * width * height * 2 // 512)

    # Finally write the updated table to the file
    f.seek(0)
    f.write(table)
    f.close()


def gif_to_bytearray(gif):

    ba = bytearray()
    for frame in ImageSequence.Iterator(gif):
        frame = frame.resize((width, height))
        frame = image_to_rgb565_bytearray(frame)
        ba.extend(frame)

    return ba


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

def to_little_endian(number, bytes):
    ba = bytearray(bytes)
    for i in range(bytes):
        ba[i] = number >> (8 * i) & 0xFF
    return ba

if __name__ == "__main__":
    main()