#!/usr/bin/python

## This program generates n files, each m bytes, in a specified directory using randomly selected bytes

import sys
import os
import random
import shutil

useOnlyAscii = True

if len(sys.argv) < 4:
    print "Usage: ./garbage dirname size_in_bytes num_files"
    sys.exit(-1)
    
dirname = sys.argv[1]
filesize = int(sys.argv[2])
num_files = int(sys.argv[3])

if not os.path.exists(dirname):
    print "Dirpath invalid"
    sys.exit(-1)

for i in range (0, num_files):
    f = open(dirname + "/" + "garbage." + str(i), 'w')
    for j in range(0, filesize):
        if useOnlyAscii :
            rand_char = chr(random.randint(32, 127))
        else :
            rand_char = chr(random.randint(0, 255))
        f.write(rand_char)    
    f.close()