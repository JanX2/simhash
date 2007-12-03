#!/usr/bin/python

## This program cycles through the given directory and deletes a file if it has any identical repeats

import sys
import os
import shutil
import md5

if len(sys.argv) < 2:
    print "Usage: ./dumb_del_diffs dirname"
    sys.exit(-1)
    
dirname = sys.argv[1]

if not os.path.exists(dirname):
    print "Dirpath invalid"
    sys.exit(-1)

filesBySize = {}

def walker(arg, dirname, fnames):
    d = os.getcwd()
    os.chdir(dirname)       
    for f in fnames:
        if not os.path.isfile(f):
            continue
        size = os.stat(f).st_size
     #   if size < 100:
     #       continue
        if filesBySize.has_key(size):
            a = filesBySize[size]
        else:
            a = []
            filesBySize[size] = a
        a.append(os.path.join(dirname, f))
    os.chdir(d)


print 'Scanning directory "%s"....' % dirname
os.path.walk(dirname, walker, filesBySize)    

print 'Finding potential dupes...'
potentialDupes = []
potentialCount = 0
trueType = type(True)
sizes = filesBySize.keys()
sizes.sort()
for k in sizes:
    inFiles = filesBySize[k]
    outFiles = []
    hashes = {}
    if len(inFiles) is 1: continue
    print 'Testing %d files of size %d...' % (len(inFiles), k)
    for fileName in inFiles:
        if not os.path.isfile(fileName):
            continue
        aFile = file(fileName, 'r')
        hasher = md5.new(aFile.read(1024))
        hashValue = hasher.digest()
        if hashes.has_key(hashValue):
            x = hashes[hashValue]
            if type(x) is not trueType:
                outFiles.append(hashes[hashValue])
                hashes[hashValue] = True
            outFiles.append(fileName)
        else:
            hashes[hashValue] = fileName
        aFile.close()
    if len(outFiles):
        potentialDupes.append(outFiles)
        potentialCount = potentialCount + len(outFiles)
del filesBySize

print 'Found %d sets of potential dupes...' % potentialCount
print 'Scanning for real dupes...'

dupes = []
for aSet in potentialDupes:
    outFiles = []
    hashes = {}
    for fileName in aSet:
        print 'Scanning file "%s"...' % fileName
        aFile = file(fileName, 'r')
        hasher = md5.new()
        while True:
            r = aFile.read(4096)
            if not len(r):
                break
            hasher.update(r)
        aFile.close()
        hashValue = hasher.digest()
        if hashes.has_key(hashValue):
            if not len(outFiles):
                outFiles.append(hashes[hashValue])
            outFiles.append(fileName)
        else:
            hashes[hashValue] = fileName
    if len(outFiles):
        dupes.append(outFiles)

i = 0
for d in dupes:
    print 'Original is %s' % d[0]
    for f in d[1:]:
        i = i + 1
        print 'Deleting %s' % f
        os.remove(f)
    print