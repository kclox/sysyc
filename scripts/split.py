import sys
import os

file = sys.argv[1]
out = file + ".split"


def split_file(file, out, block_size=0x1000000):
    os.system("mkdir %s" % out)
    fp = open(file, "rb")
    i = 0
    while True:
        buf = fp.read(block_size)
        if len(buf) > 0:
            print(i)
            with open(out + "/%d" % i, "wb") as fo:
                fo.write(buf)
        if len(buf) < block_size:
            break
        i += 1


split_file(file, out)
