#! /bin/zsh

paths=.
dir=${0%/*}
test $dir = . || paths+=:$dir
PATH=$paths:$PATH

base=$1
shift

. $JOUST

setopt nullglob
for subdir in $@ ; do
    pushd $srcdir
    if ! test -x $subdir/enable.sh || $subdir/enable.sh ; then
	echo $subdir/*(.^*)
    fi
    popd
done | aloy -o $base kratos
