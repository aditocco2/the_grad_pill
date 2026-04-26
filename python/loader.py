import os
import math
import shutil
from image_proc import *

# ------------------- User Parameters ---------------------

media_dir = r"C:\Users\pluh\Downloads\responses_4_25"
sd_file = "raw/media.bin"

width, height = 64, 64
switch_interval = 10
stretch = True
randomize = True

use_static_mode = True
static_mode_image = "media_static/happy.gif"

# ---------------------------------------------------------

def main():
    global use_static_mode

    media_files = [f"{media_dir}/{f}" for f in os.listdir(media_dir)]
    file_count = len(media_files) + (1 if use_static_mode else 0)

    data_file = open("temp_data.bin", "wb")

    # Keep all media info, as we need to dynamically change sector addresses later
    media_info_list = []

    # Keep track of how many files were successfully processed
    num_media = 0

    RED = '\033[91m'
    RESET = '\033[0m'
    # Slideshow Mode
    for i, file in enumerate(media_files):
        try:
            media_info, data_array = process_media(file)
            media_info_list.append(media_info)
            data_file.write(data_array)
            num_media += 1
            print(f"Processed {file} ({i+1}/{file_count})")

        except ProcessingFailedError:
            print(f"{RED}Failed to process {file} ({i+1}/{file_count}){RESET}")
        except FileNotSupportedError:
            print(f"{RED}File {file} not supported ({i+1}/{file_count}){RESET}")
    
    # Static Mode
    if(use_static_mode):
        try:
            media_info, data_array = process_media(static_mode_image)
            media_info_list.append(media_info)
            data_file.write(data_array)
            num_media += 1
            print(f"Processed {file} for static mode ({file_count}/{file_count})")

        except ProcessingFailedError:
            print(f"{RED}Failed to process {file} ({file_count}/{file_count}). Disabling static mode")
            use_static_mode = False
        except FileNotSupportedError:
            print(f"{RED}File {file} not supported ({file_count}/{file_count}). Disabling static mode")
            use_static_mode = False
    
    data_file.close()
    # We are finished writing pixel data at this point. Now for the table of contents

    # 16-byte row for each media, plus a 16-byte header
    table_sectors = math.ceil((num_media + 1) * 16 / 512)

    table = bytearray(table_sectors * 512)

    make_table_header(table, num_media)
    make_table_rows(table, table_sectors, media_info_list)

    table_file = open("temp_table.bin", "wb")
    table_file.write(table)
    table_file.close()

    # And put 'em together
    file_concat(["temp_table.bin", "temp_data.bin"], sd_file, clean=True)

    print("Done!")
    print(f"Your file is ready in {sd_file}")


def make_table_header(table, num_media):
    # Table Header Format:
    # bytes 0 to 3 are the number of pictures/videos
    # bytes 4 to 5 are how many seconds
    # byte 6 is whether to include static mode
    # byte 7 is whether to randomize playback
    table_row = bytearray(16)
    table_row[0:4] = to_little_endian(num_media, 4)
    table_row[4:6] = to_little_endian(int(switch_interval), 2)
    table_row[6] = 1 if use_static_mode else 0
    table_row[7] = 1 if randomize else 0
    table[0:16] = table_row


def make_table_rows(table, table_sectors, media_info_list):
    # Table Row Format:
    # bytes 0 to 3 are the sector address of the first frame of the video
    # bytes 4 to 7 are the number of frames in the video
    # bytes 8 to 9 are how long each frame lasts in milliseconds
    sector_num = table_sectors
    sectors_per_frame = math.ceil(width * height * 2 / 512)
    for index, info in enumerate(media_info_list):
        table_row = bytearray(16)
        table_row[0:4] = to_little_endian(sector_num, 4)
        table_row[4:8] = to_little_endian(info["n_frames"], 4)
        table_row[8:10] = to_little_endian(info["frame_time_ms"], 2)

        table[(index + 1) * 16 : (index + 2) * 16] = table_row
        sector_num += (info["n_frames"] * sectors_per_frame)


def process_media(file):

    image_types = [".png", ".jpg", ".jpeg"]
    video_types = [".mp4", ".mov", ".avi", ".mkv"]
    
    extension = os.path.splitext(file)[1].lower()
    if extension in image_types:
        try:
            data_array = image_to_bytearray(file, width, height, stretch)
            media_info = {
                "frame_time_ms": 0,
                "n_frames": 1
            }
        except:
            raise ProcessingFailedError
    elif extension in video_types:
        try:
            data_array = video_to_bytearray(file, width, height, stretch)
            media_info = {
                "frame_time_ms": get_video_frame_time_ms(file),
                "n_frames": get_video_n_frames(file)
            }
        except:
            raise ProcessingFailedError
    elif extension == ".gif":
        try:
            data_array = gif_to_bytearray(file, width, height, stretch)
            media_info = {
                "frame_time_ms": get_gif_frame_time_ms(file),
                "n_frames": get_gif_n_frames(file)
            }
        except:
            raise ProcessingFailedError
    else: # file not supported
        raise FileNotSupportedError

    return media_info, data_array


def file_concat(files_in, file_out, clean=False):
    with open(file_out, "wb") as outfile:
        for fname in files_in:
            with open(fname, "rb") as infile:
                shutil.copyfileobj(infile, outfile)
                    
    if clean:
        for f in files_in:
            os.remove(f)


def to_little_endian(number, bytes):
    ba = bytearray(bytes)
    for i in range(bytes):
        ba[i] = number >> (8 * i) & 0xFF
    return ba


if __name__ == "__main__":
    main()