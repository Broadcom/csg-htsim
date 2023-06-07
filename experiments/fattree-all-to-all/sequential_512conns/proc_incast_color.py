import sys
filename = sys.argv[1]
target = int(sys.argv[2])
#print(filename)
file = open(filename, "r")
fparts = filename.split(".")
assert(len(fparts) > 1)
last = len(fparts)-1
fparts.append(fparts[last])
fparts[last] = "color"
ofilename = ".".join(fparts)
#print(ofilename)
ofile = open(ofilename, "w")
for line in file:
    parts = line.split()
    if len(parts) < 1:
        #print("", file=ofile)
        continue
    height=float(parts[2])
    if height <= target:
        col = 0
    elif height <= 1.5 * target:
        col = 1
    elif height <= 2 * target:
        col = 2
    else:
        col = 3
    print(parts[0], parts[1], parts[2], col, file=ofile)
    
