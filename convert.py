from PIL import Image
import sys
from struct import *

fname = sys.argv[1]
oname = sys.argv[2]

im = Image.open(fname)


width, height = im.size

out = ''

out += pack('ii', width, height)
for y in range(height):
    for x in range(width):
        r, g, b, a = im.getpixel((x, y))
        # pack this into memory correctly.
        out += pack('BBBB', b, g, r, a)


with open(oname, 'w') as f:
    f.write(out)
    f.close()
