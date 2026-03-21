from PIL import Image, ImageSequence, ImageOps
from moviepy import VideoFileClip

def get_video_n_frames(file):
    with VideoFileClip(file) as clip:
        n_frames = clip.n_frames
    return n_frames

def get_gif_n_frames(file):
    with Image.open(file) as image:
        n_frames = image.n_frames
    return n_frames

def get_video_frame_time_ms(file):
    with VideoFileClip(file) as clip:
        frametime = int(1000/clip.fps)
    return frametime

def get_gif_frame_time_ms(file):
    with Image.open(file) as image:
        frametime = image.info["duration"]
    return frametime

def video_to_bytearray(file, width, height, stretch=True):

    ba = bytearray()
    with VideoFileClip(file) as clip:
        for frame in clip.iter_frames():
            frame = Image.fromarray(frame)
            frame = image_to_bytearray(frame, width, height, stretch)
            ba.extend(frame)
    
    return ba
        

def gif_to_bytearray(file, width, height, stretch=True):

    ba = bytearray()
    with Image.open(file) as image:
        for frame in ImageSequence.Iterator(image):
            frame = image_to_bytearray(frame, width, height, stretch)
            ba.extend(frame)

    return ba


def image_to_bytearray(image, width, height, stretch=True):

    # If a file is passed in, open it in PIL first
    if type(image) is str:
        image = Image.open(image)
    
    if stretch:
        image = image.resize((width, height), Image.Resampling.NEAREST)
    else:
        image = ImageOps.pad(image, (width, height), Image.Resampling.NEAREST)
                             
    # Convert to RGB888 if not already RGB888
    image = image.convert("RGB")

    # Convert to RGB565 
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
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

class FileNotSupportedError(Exception):
    """File not supported"""
    pass

class ProcessingFailedError(Exception):
    """Image processing failed"""
    pass