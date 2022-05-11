import sys
import os

dir = sys.argv[1]
out = dir + ".out"
fout = open(out, "wb")

for _, _, files in os.walk(dir):
    n = len(files)
    for i in range(n):
        with open(dir + "/" + str(i), "rb") as fp:
            buf = fp.read()
            fout.write(buf)

fout.close()
