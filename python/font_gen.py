# Converts a TTF font or PNG file to a bitmap header
# Adapted from Jared Sanson's FontGen script: https://jared.geek.nz/2014/01/custom-fonts-for-microcontrollers/

from PIL import Image, ImageFont, ImageDraw
import os.path

def main():

    # font = {'fname': r'AtariST.ttf', 'size': 16, 'yoff': 0, 'xoff': -1, 'w': 8, 'h': 16}
    font = {'fname': r'Portfolio.ttf', 'size': 8, 'yoff': 0, 'xoff': 0, 'w': 6, 'h': 8}
    png, w, h = ttf_to_png(font)
    font_png_to_header(png, w, h)
    
    # font_png_to_header("3x5_font.png", 4, 6)
    

def ttf_to_png(font_info):

    font_file = font_info['fname']
    font_size = font_info['size']
    font_y_offset = font_info.get('yoff', 0)
    font_x_offset = font_info.get('xoff', 0)

    char_width = font_info.get('w', 5)
    char_height = font_info.get('h', 8)
    glyph_width = char_width + 1

    font_begin = ' '
    font_end = '~'
    font_str = ''.join(chr(x) for x in range(ord(font_begin), ord(font_end)+1))

    output_png = os.path.splitext(font_file)[0] + '.png'

    image_width = (char_width + 1) * len(font_str)
    image_height = char_height

    img = Image.new("RGBA", (image_width, image_height), (255,255,255))
    fnt = ImageFont.truetype(font_file, font_size)

    drw = ImageDraw.Draw(img)

    for i in range(len(font_str)):
        drw.text((i*glyph_width+font_x_offset,font_y_offset), font_str[i], (0,0,0), font=fnt)

    img.save(output_png)

    print(f"Converted {font_file} to {output_png}")

    return output_png, char_width, char_height

def font_png_to_header(png_file, char_width, char_height):

    font_begin = ' '
    font_end = '~'
    font_str = ''.join(chr(x) for x in range(ord(font_begin), ord(font_end)+1))

    img = Image.open(png_file)
    img = img.convert("RGB")

    header_file = os.path.splitext(png_file)[0] + ".h"
    f = open(header_file, 'w')
    num_chars = len(font_str)

    f.write('const uint8_t font[%d * %d] = {\n' % (num_chars+1, char_height))

    for i in range(num_chars):
        ints = []
        for y in range(char_height):
            val = 0
            for relative_x in range(char_width):
                absolute_x = i*(char_width+1) + relative_x
                rgb = img.getpixel((absolute_x, y))
                val = (val << 1) | (1 if rgb[0] == 0 else 0)

            ints.append('0x%.2x' % (val))
        c = font_str[i]
        if c == '\\': c = '"\\"' # bugfix
        f.write('\t%s, // %s\n' % (','.join(ints), c))

    f.write('\t%s\n' % (','.join(['0x00']*char_height)))
    f.write('};\n\n')

    f.close()

    print(f"Converted {png_file} to {header_file}")

if __name__ == "__main__":
    main()