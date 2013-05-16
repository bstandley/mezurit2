#!/bin/bash

###################  required inputs  #####################

#  $prog1 (lowercase), $prog2 (proper name), $version
#  $srcdir, $rules, $shares
#  $option_flags, $extra_flags, $ldlibs

###################  utility functions  ###################

load_defaults ()
{
	prefix=/usr
	bindir=bin
	libdir=lib
	sharedir=share
	win32_path=../win32

	# switches:
	do_regen=false
	mingw=false
	gtk_type=gtk2
	header_warnings=false

	# compiler flags:
	COMMON_FLAGS="-ansi --std=c99 -pedantic -Wall -O2 -funroll-loops"
	EXTRA_WARNINGS="-Wextra -Wno-unused-parameter -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wconversion"
	EXTRA_FLAGS=""
	LDLIBS=""
}

mention_standard_options ()
{
	echo -e "Usage: $0 [OPTION...]\n"
	echo -e "  --regen                 regenerate Makefile.dep (requires perl)\n"
	echo -e "  --prefix=[PATH]         specify installation path (not applicable to MinGW builds)\n"
	echo -e "  --mingw                 use the MinGW compiler to generate Windows binaries (32-bit only)\n"
	echo -e "  --win32path=[PATH]      specify path to win32 libraries (default: $win32_path)\n"
	echo -e "  --headerwarnings        do not suppress compiler warnings generated by system headers e.g. GTK\n"
	echo -e "  --gtk3                  use GTK+-3.x rather than GTK+-2.y\n"
}

handle_standard_options ()
{
	if ! $found ; then
		if   [[ $1 == '--regen'          ]] ; then do_regen=true
		elif [[ $1 =~ '--prefix'         ]] ; then prefix=$(echo $1 | sed "s/--prefix=//")
		elif [[ $1 == '--mingw'          ]] ; then mingw=true ; prefix=N/A
		elif [[ $1 =~ '--win32path'      ]] ; then win32_path=$(echo $1 | sed "s/--win32path=//")
		elif [[ $1 == '--headerwarnings' ]] ; then header_warnings=true
		elif [[ $1 == '--gtk3'           ]] ; then gtk_type=gtk3
		else
			echo -e "Unrecognized option: $1\n"
			$0 --help
			exit 1
		fi
	fi
}

detect_system ()
{
	if $mingw ; then arch='i686' ; opt_system='MinGW (32-bit)'
	else
		if [[ $(uname -m) == "x86_64" ]] ; then arch='x86-64' ; opt_system='Linux x86_64       (Auto-detected)'
		else                                    arch='i686'   ; opt_system='Linux i686         (Auto-detected)' ; fi
	fi
}

detect_python ()
{
	if $mingw ; then
		pydll=$(find $win32_path -name python*.dll -printf %f)
		pylib=$(basename $pydll .dll)
		EXTRA_FLAGS="$EXTRA_FLAGS -I$(find $win32_path -name Python.h -printf %h)"
		LDLIBS="$LDLIBS -L$(find $win32_path -name lib$pylib.a -printf %h) -l$pylib"
	else
#		pyver=3.2
#		which python$pyver-config > /dev/null
		/bin/false
		if [ $? != 0 ] ; then pyver=2.7 ; fi
		which python$pyver-config > /dev/null
		if [ $? != 0 ] ; then pyver=2.6 ; fi

		EXTRA_FLAGS="$EXTRA_FLAGS $(python$pyver-config --includes) -fPIC"
		LDLIBS="$LDLIBS $(python$pyver-config --libs) -L$(find /usr/lib/python$pyver -name libpython$pyver.a -printf %h)"
	fi
}

detect_gtk ()
{
	if [[ $gtk_type == 'gtk3' ]] ; then gtk_pkg=gtk+-3.0 ; vte_pkg=vte-2.90
	else                                gtk_pkg=gtk+-2.0 ; vte_pkg=vte      ; fi

	if $mingw ; then
		pc_path=$(find $win32_path -name $gtk_pkg.pc -printf %h)
		vte_pkg=  # VTE is not available on win32 :(
	fi

	glib_version=$(PKG_CONFIG_PATH=$pc_path pkg-config --modversion glib-2.0)
	gtk_version=$(PKG_CONFIG_PATH=$pc_path pkg-config --modversion $gtk_pkg)

	EXTRA_FLAGS="$EXTRA_FLAGS $(PKG_CONFIG_PATH=$pc_path pkg-config --cflags $gtk_pkg gthread-2.0 $vte_pkg) -DG_DISABLE_DEPRECATED=1 -DGTK_DISABLE_DEPRECATED=1 -DGTK_MULTIHEAD_SAFE=1"
	LDLIBS="$LDLIBS $(PKG_CONFIG_PATH=$pc_path pkg-config --libs $gtk_pkg gthread-2.0 $vte_pkg)"
}

write_makefile_dep ()
{
	if [ ! -e Makefile.dep ] ; then
		echo "Makefile.dep does not exist, regenerating."
		do_regen=true
	fi

	if $do_regen ; then
		echo "Regenerating dependencies..."

		echo "# Makefile.dep"    >  Makefile.dep
		echo "# Generated by $0" >> Makefile.dep
		echo "# $(date)"         >> Makefile.dep

		i=0
		for x in $rules ; do
			y=($(echo $x | tr ':' ' '))
			
			if [[ ${y[0]} == 'app' || ${y[0]} == 'lib' ]] ; then
				objs[$i]=$(utils/find_deps.pl --objects ${y[2]})
				echo "${y[2]}_OBJ = ${objs[$i]}" >> Makefile.dep
				i=$(($i+1))
			fi
		done
		all_obj=$(utils/find_deps.pl --list ${objs[@]})  # remove duplicates

		echo -e "\nALL_OBJ = $all_obj\n"      >> Makefile.dep
		utils/find_deps.pl --targets $all_obj >> Makefile.dep

		echo -e "\nWrote Makefile.dep"
	fi
}

write_makefile ()
{
	OPTION_FLAGS="-DPROG1=$prog1 -DPROG2=$prog2 -DVERSION=$version"
	if $header_warnings ; then OPTION_FLAGS="$OPTION_FLAGS -DSHOW_HEADER_WARNINGS" ; fi

	if ! $mingw ; then
		binpath=$prefix/$bindir
		libpath=$prefix/$libdir/$prog1
		sharepath=$prefix/$sharedir/$prog1

		OPTION_FLAGS="$OPTION_FLAGS -DBINPATH=$binpath -DLIBPATH=$libpath -DSHAREPATH=$sharepath"
		cc='gcc'
		real_install="install"
	else
		OPTION_FLAGS="$OPTION_FLAGS -DMINGW"
		cc='i486-mingw32-gcc'
	fi

	echo "# Makefile"        >  Makefile
	echo "# Generated by $0" >> Makefile
	echo "# $(date)"         >> Makefile

	if ! $mingw ; then
		echo >> Makefile
		echo "BINPATH   = \$(DESTDIR)$binpath"   >> Makefile
		echo "LIBPATH   = \$(DESTDIR)$libpath"   >> Makefile
		echo "SHAREPATH = \$(DESTDIR)$sharepath" >> Makefile
	fi

	for x in $rules ; do
		y=($(echo $x | tr ':' ' '))
		ALL_TARGETS="$ALL_TARGETS ${y[1]}"
	done

	echo "
COMMON_FLAGS   = $COMMON_FLAGS
OPTION_FLAGS   = $OPTION_FLAGS $option_flags
EXTRA_WARNINGS = $EXTRA_WARNINGS
EXTRA_FLAGS    = $EXTRA_FLAGS $extra_flags

CFLAGS = \$(COMMON_FLAGS) -march=$arch \$(OPTION_FLAGS) \$(EXTRA_WARNINGS) -I$srcdir \$(EXTRA_FLAGS)
LD_FLAGS =
LDLIBS = $LDLIBS $ldlibs

ALL_TARGETS = $ALL_TARGETS

CC = $cc
INSTALL = install

.PHONY : all clean $real_install install_local

all : \$(ALL_TARGETS)

clean :
	@rm -f \$(ALL_OBJ) \$(ALL_TARGETS)

%.o : %.c
	@echo \"  CC   \$(patsubst $srcdir/%,%,\$@)\"
	@\$(CC) \$(CFLAGS) -o \$@ -c \$<

include Makefile.dep
" >> Makefile

	for x in $rules ; do
		y=($(echo $x | tr ':' ' '))

		if [[ ${y[0]} == 'app' ]] ; then bin_files="$bin_files ${y[1]}" ; else lib_files="$lib_files ${y[1]}" ; fi
		if [[ ${y[0]} == 'lib' ]] ; then shared="-shared"               ; else shared=""                      ; fi

		if [[ ${y[0]} == 'copy' ]] ; then
			echo -e "${y[1]} : ${y[2]}"                               >> Makefile
			echo -e "\t@echo \"  CP   \$(patsubst $srcdir/%,%,\$@)\"" >> Makefile
			echo -e "\t@\$(INSTALL) \$< \$@\n"                        >> Makefile
		else
			echo -e "${y[1]} : \$(${y[2]}_OBJ)"                              >> Makefile
			echo -e "\t@echo \"  LD   \$(patsubst $srcdir/%,%,\$@)\""        >> Makefile
			echo -e "\t@\$(CC) \$(LD_FLAGS) $shared -o \$@ \$^ \$(LDLIBS)\n" >> Makefile
		fi
	done

	if ! $mingw ; then
		write_install_rule "install" "\$(BINPATH)" "\$(LIBPATH)" "\$(SHAREPATH)"
		write_install_rule "install_local" $prog1 $prog1 $prog1
	else
		write_install_rule "install_local" $prog2 $prog2 $prog2
	fi

	echo "Wrote Makefile, now run make..."
}

write_install_rule ()
{
	echo "$1 : \$(ALL_TARGETS)" >> Makefile
	for x in $bin_files ; do
		printf "\t\$(INSTALL) -D -m 755 %-60s %s\n" $x $2/$(basename $x) >> Makefile
	done
	for x in $lib_files ; do
		printf "\t\$(INSTALL) -D -m 644 %-60s %s\n" $x $3/$(basename $x) >> Makefile
	done
	for x in $shares ; do
		y=($(echo $x | tr ':' ' '))
		printf "\t\$(INSTALL) -D -m 644 %-60s %s\n" ${y[1]} $4/${y[0]} >> Makefile
	done
	echo >> Makefile
}
