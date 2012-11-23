#!/bin/bash

# Usage: Supply path to pixmaps as an argument.

for file in `ls $1/*.svg` ; do
	base=`basename $file .svg`
	rsvg-convert -f png $file -o $1/$base.png
done
