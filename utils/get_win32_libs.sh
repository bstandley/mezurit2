#!/bin/bash

if [[ $# == 1 ]] ; then
	target=$1
	[[ -d $target ]] || mkdir $target
else
	echo -e "Usage: $0 [absolute install path]\n"
	exit 1
fi

py_url=http://python.org/ftp/python/
py_version=2.7.2

pyrl_url=http://pypi.python.org/packages/source/p/pyreadline/
pyrl_version=1.7.1

gtk_url=http://ftp.gnome.org/pub/gnome/binaries/win32

pkgs=( glib atk  pango gdk-pixbuf gtk+)
vers=( 2.28 1.32 1.29  2.24       2.24)
xvers=(8-1  0-2  4-1   0-1        10-1)

dep_pkgs=( zlib cairo libpng freetype fontconfig expat gettext-runtime)
dep_vers=( 1.2  1.10  1.4    2.4      2.8        2.0   0.18.1)
dep_xvers=(5-2  2-2   3-1    2-1      0-2        1-1   1-2)

echo -e "\n####  OBTAINING GTK2  ####\n"

get_and_unzip ()
{
	wget -nc -P /tmp $1/$2 || exit 1
	echo -e "Unzipping /tmp/$2...\n"
	unzip -qo /tmp/$2 -d $target/gtk2
}

i=0
for pkg in ${pkgs[@]} ; do
	get_and_unzip $gtk_url/$pkg/${vers[$i]} ${pkg}_${vers[$i]}.${xvers[$i]}_win32.zip
	get_and_unzip $gtk_url/$pkg/${vers[$i]} ${pkg}-dev_${vers[$i]}.${xvers[$i]}_win32.zip
	i=$(($i+1))
done

i=0
for dep_pkg in ${dep_pkgs[@]} ; do
	get_and_unzip $gtk_url/dependencies ${dep_pkg}_${dep_vers[$i]}.${dep_xvers[$i]}_win32.zip
	get_and_unzip $gtk_url/dependencies ${dep_pkg}-dev_${dep_vers[$i]}.${dep_xvers[$i]}_win32.zip
	i=$(($i+1))
done

utils/fix_pc_files.pl $target/gtk2 $target/gtk2/lib/pkgconfig/*.pc

echo -e "\n####  OBTAINING PYTHON  ####\n"

mkdir /tmp/wine_msi_temp
wget -nc -P /tmp $py_url/$py_version/python-$py_version.msi || exit 1
WINEPREFIX=/tmp/wine_msi_temp msiexec /a /tmp/python-$py_version.msi /qb TARGETDIR=$target/python2
rm -r /tmp/wine_msi_temp

echo -e "\n####  OBTAINING PYREADLINE  ####\n"

wget -nc -P /tmp $pyrl_url/pyreadline-$pyrl_version.zip || exit 1
unzip -qo /tmp/pyreadline-$pyrl_version.zip -d /tmp
cp -rT /tmp/pyreadline-$pyrl_version $target/pyreadline

echo -e "\n####  DONE  ####\n"
