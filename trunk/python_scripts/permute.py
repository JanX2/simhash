#!/usr/bin/python

## This program takes a file, picks n offsets at random, replaces 
## the bytes at those offsets with a random byte and then saves 
## the file with a ++name, and then repeats on the new file, k 
## times

import sys
import os
import random
import shutil

if len(sys.argv) < 5:
    print "Usage: ./permute filename number_bytes num_files dirname"
    sys.exit(-1)
    
filepath = sys.argv[1]
num_bytes = int(sys.argv[2])
num_files = int(sys.argv[3])
dirname = sys.argv[4]

filename = os.path.split(filepath)[1]

if not os.path.exists(filepath):
    print "Filepath invalid."
    sys.exit(-1)

if not os.path.exists(dirname):
    print "Dirpath invalid"
    sys.exit(-1)
    
filesize = os.stat(filepath).st_size

def permute_file(filepath):
    global num_bytes, filesize
    f = open(filepath, 'r+')
    for i in range(0, num_bytes):
        offset = random.randint(0, filesize - 1)
        rand_char = chr(random.randint(0, 255))
    
        f.seek(offset)
        f.write(rand_char)    
    f.close()
    
shutil.copyfile(filepath, dirname + "/0_"+filename)

for i in range(1, num_files):
    prev = dirname + "/" + str(i - 1) + "_" + filename
    curr = dirname + "/" + str(i) + "_" + filename
    shutil.copyfile(prev, curr)
    permute_file(curr)
    


