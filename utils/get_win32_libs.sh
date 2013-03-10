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

gtk2_url=http://ftp.gnome.org/pub/gnome/binaries/win32
gtk3_url=http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_Factory/noarch/

gtk2_pkgs=(                                                                             \
	glib/2.28/glib_2.28.8-1                 glib/2.28/glib-dev_2.28.8-1                 \
	atk/1.32/atk_1.32.0-2                   atk/1.32/atk-dev_1.32.0-2                   \
	pango/1.29/pango_1.29.4-1               pango/1.29/pango-dev_1.29.4-1               \
	gdk-pixbuf/2.24/gdk-pixbuf_2.24.0-1     gdk-pixbuf/2.24/gdk-pixbuf-dev_2.24.0-1     \
	gtk+/2.24/gtk+_2.24.10-1                gtk+/2.24/gtk+-dev_2.24.10-1                \
	dependencies/zlib_1.2.5-2               dependencies/zlib-dev_1.2.5-2               \
	dependencies/cairo_1.10.2-2             dependencies/cairo-dev_1.10.2-2             \
	dependencies/libpng_1.4.3-1             dependencies/libpng-dev_1.4.3-1             \
	dependencies/freetype_2.4.2-1           dependencies/freetype-dev_2.4.2-1           \
	dependencies/fontconfig_2.8.0-2         dependencies/fontconfig-dev_2.8.0-2         \
	dependencies/expat_2.0.1-1              dependencies/expat-dev_2.0.1-1              \
	dependencies/gettext-runtime_0.18.1.1-2 dependencies/gettext-runtime-dev_0.18.1.1-2)

gtk3_pkgs=(                                                       \
	glib2-2.34.1-1.53               glib2-devel-2.34.1-1.53       \
	atk-2.6.0-1.73                  atk-devel-2.6.0-1.73          \
	pango-1.30.1-1.84               pango-devel-1.30.1-1.84       \
	gdk-pixbuf-2.26.3-1.83          gdk-pixbuf-devel-2.26.3-1.83  \
	gtk3-3.6.1-1.53                 gtk3-devel-3.6.1-1.53         \
	zlib-1.2.7-1.137                zlib-devel-1.2.7-1.137        \
	libcairo2-1.10.2-8.136          cairo-devel-1.10.2-8.136      \
	libpng-1.5.11-1.119             libpng-devel-1.5.11-1.119     \
	freetype-2.4.10-1.118           freetype-devel-2.4.10-1.118   \
	fontconfig-2.10.1-1.102         fontconfig-devel-2.10.1-1.102 \
	libexpat-2.0.1-4.288            libexpat-devel-2.0.1-4.288    \
	libintl-0.18.1.1-13.262         libintl-devel-0.18.1.1-13.262 \
	pixman-0.26.0-1.129             pixman-devel-0.26.0-1.129     \
	gettext-runtime-0.18.1.1-13.262)

echo -e "\n####  OBTAINING GTK2  ####\n"

for pkg in ${gtk2_pkgs[@]} ; do
	pkgf=${pkg}_win32.zip
	wget -nc -P /tmp $gtk2_url/$pkgf || exit 1
	pkgb=$(basename $pkgf)
	echo -e "Unzipping /tmp/$pkgb...\n"
	unzip -qo /tmp/$pkgb -d $target/gtk2
done

utils/fix_pc_files.pl $target/gtk2 $target/gtk2/lib/pkgconfig/*.pc

echo -e "\n####  OBTAINING GTK3  ####\n"

for pkg in ${gtk3_pkgs[@]} ; do
	pkgf=mingw32-${pkg}.noarch.rpm
	wget -nc -P /tmp $gtk3_url/$pkgf || exit 1
	echo -e "Extracting /tmp/$pkgf...\n"
	rpm2cpio /tmp/$pkgf | bsdtar -xf - -C /tmp
done

mv /tmp/usr/i686-w64-mingw32/sys-root/mingw $target/gtk3
utils/fix_pc_files.pl $target/gtk3 $target/gtk3/lib/pkgconfig/*.pc

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
