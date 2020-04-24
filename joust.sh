#! /bin/zsh

paths=.
dir=${0%/*}
test $dir = . || paths+=:$dir
PATH=$paths:$PATH

base=$1
shift

. $JOUST

$srcdir/jouster "$@" | aloy -o $base kratos
