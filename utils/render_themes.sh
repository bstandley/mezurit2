#!/bin/bash

# Usage: Supply path to theme templates as an argument.

for file in `ls $1/*.css.j2` ; do
	base=`basename $file .css.j2`
	for x in linux windows ; do
		system=$x j2 $file -o $1/$base-$x.css
	done
done
