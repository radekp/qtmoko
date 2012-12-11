#!/bin/bash

# TODO:
#
# Identify user-installed packages and offer to reinstall these after
# an upgrade.  (In my case: rsync, sgt-puzzles.)

INCL=`cat <<EOF
/etc/ssh/*key*
/home/root
/home/root/Applications/Network/config
/home/root/Applications/qtopiamail
EOF
`
EXCL=`cat <<EOF
/home/root/.mplayer/config
/home/root/.pulse
/home/root/.pulse-cookie
/home/root/Applications
/home/root/Applications/qtopiamail/temp
EOF
`
echo INCL:
for i in $INCL; do echo $i; done

echo EXCL:
for e in $EXCL; do echo $e; done

BDIR=/media/card/Backup/`date +%F`
echo Backing up to $BDIR
mkdir -p $BDIR

backup ()
{
    incl_root=$1
    file=$2
    do_backup=true

    for e in $EXCL; do
	case $incl_root in
	    ${e}/* )
                # Include root is more specific than exclusion, so
                # overrides it.
                ;;
	    * )
	        # Include root does not override this exclusion.
	        case $file in
		    $e | ${e}/* )
		        # Don't backup this file.
		        do_backup=false
			;;
		esac
		;;
	esac
    done

    if $do_backup; then
	echo -en ${file}\\0000
    fi
}

for i in $INCL; do

    if test -f $i; then
	backup $i $i
    elif test -d $i; then
	OIFS="$IFS"
	IFS=$'\n' 
	for f in `find $i -type f`; do
	    backup $i "$f"
	done
	IFS="$OIFS"
    else
	echo "E: Don't know how to backup $i" >&2
    fi

done | xargs -0 tar -rf $BDIR/files.tar
