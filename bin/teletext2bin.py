#!/usr/bin/python

import sys

if len(sys.argv) != 3:
    print "teletext2bin.py <infile> <outfile>"
    exit()

infile = sys.argv[1]
outfile = sys.argv[2]

print "converting " + infile + " to " + outfile

fh = open(infile, 'rb')

ba = bytearray(fh.read())
fh.close()
bo = bytearray()
for byte in ba:
    if byte != 10 and byte != 13:
        bo.append(byte)


print "length " + str(len(bo)) + " bytes"

fh = open(outfile, 'wb')
fh.write(bo)
fh.close()

print "Done."
