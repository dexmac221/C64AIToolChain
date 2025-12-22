import math
import sys
from PIL import Image

im = Image.open(sys.argv[1])

ii = 0
jj = 0
ar = ""
ra = ""
counter = 0
maxcolor = {}

colors = (
    (0, 0, 0), (255, 255, 255), (136, 0, 0),
    (170, 255, 238), (204, 68, 204), (0, 204, 85),
    (0, 0, 170), (238, 238, 119), (221, 136, 85),
    (102, 68, 0), (255, 119, 119), (51, 51, 51),
    (119, 119, 119), (170, 255, 102), (0, 136, 255),
    (187, 187, 187))


def find_color(test_color):
    indx = 0
    min_col = 0
    min_diff = 1000

    for color in colors:
        #print (str(color))
        d0 = (float(color[0])-test_color[0])
        d1 = (float(color[1])-test_color[1])
        d2 = (float(color[2])-test_color[2])

        dist = math.sqrt(d0*d0+d1*d1+d2*d2)
        #print (str(d0)+','+str(d1)+','+str(d2))

        if dist < min_diff:
            min_diff = dist
            # print(str(indx)+","+str(min_diff))
            min_col = indx
        indx += 1

    return min_col


clrs = ""

for i in range(25):
    for j in range(40):
        sv = ""

        color = (0, 0, 0)
        for y in range(8):
            bx = 0
            vf = 128
            for x in range(8):
                c = im.getpixel((x+ii, y+jj))
                md = (c[0]+c[1]+c[2])/3

                color = (color[0]+c[0], color[1]+c[1], color[2]+c[2])

                if (md > 196):
                    bx |= 0
                else:
                    bx |= vf
                vf >>= 1
            sv += str(bx) + ","

        color = (color[0]/64, color[1]/64, color[2]/64)
        #print ("clr:"+str(color))
        clrs += str(find_color(color))+','

        # sys.exit(0)

        if ((sv in maxcolor) is False):
            if (len(maxcolor.keys()) <= 256):
                maxcolor[sv] = counter
                counter += 1
                ra += sv
            else:
                repval = 255 * 8
                repind = -1
                kk = maxcolor.keys()
                ni = sv[0:len(sv) - 1].split(",")

                for k in kk:
                    cursum = 0
                    na = k[0:len(k) - 1].split(",")
                    nv = [0] * 8
                    for i in range(len(na)):
                        nv[i] = abs(eval(ni[i]) - eval(na[i]))
                    if (sum(nv) < repval):
                        rapind = k
                        repval = sum(nv)
                sv = rapind

        ar += str(maxcolor[sv]) + ","
        ii += 8

    ii = 0
    jj += 8
    ra += "\n"

f = open("./img.h", "w")
ar = ar[0:len(ar) - 1]
ar = "const unsigned char img[]={" + ar + "};"
f.write(ar)
f.close()

# Pad charmap to exactly 256 characters (2048 bytes)
# ra contains comma-separated bytes, we need to ensure it has 256*8=2048 values
ra_values = ra.replace("\n", "").rstrip(",").split(",")
ra_values = [v.strip() for v in ra_values if v.strip()]
# Pad with zeros if less than 2048 bytes
while len(ra_values) < 2048:
    ra_values.append("0")
ra = ",".join(ra_values[:2048])

f = open("./charmap.h", "w")
ra = "const unsigned char charmap[]={" + ra + "};"
f.write(ra)
f.close()

f = open("./clrs.h", "w")
clrs = clrs[0:len(clrs) - 1]
clrs = "const unsigned char clrs[]={" + clrs + "};"
f.write(clrs)
f.close()

#print(clrs)
print(counter)
