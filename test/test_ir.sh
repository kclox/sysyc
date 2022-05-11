#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

sysycc=build/src/sysycc

tmp_dir=/tmp/test_ir
mkdir -p $tmp_dir

for x in `find -L testcases/ | grep "\.sy$"`;
do
    file=`basename $x`
    ir=$tmp_dir/$file".ll"
    out=/tmp/tmp.out
    echo $x $ir
    cmd="$sysycc --emit-ir --ir $ir -L libsysy $x"
    $cmd
    if [ $? -ne 0 ]
    then
        echo $cmd
        exit 1
    fi
    # clang $ir -o $out
    # $out
done
