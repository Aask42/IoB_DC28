import sys, os

#uncomment appropriate lines for version and make sure path is valid

#flashimages = open("flash-data-v1.txt", "r").read()
#output = "d:\code\IoB_DC28\defcell.bin"

flashimages = open("flash-data-v2.txt", "r").read()
output = "d:\code\IoB_DC28\defcell-quantum.bin"

print("Read flash data file")

fout = open(output, "wb")
flashimages = flashimages.split("\n")
for lineentry in flashimages:
    if len(lineentry) == 0:
        continue

    linedata = lineentry.split(" ")

    print("Opening %s" % (" ".join(linedata[1:])))
    indata = open(" ".join(linedata[1:]),"rb").read()
    newoffset = int(linedata[0], 16)
    print("Writing %04x bytes to 0x%08x (%s)" % (len(indata), newoffset, " ".join(linedata[1:])))
    fout.seek(newoffset - 0x1000)
    fout.write(indata)

fout.close()
