#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

# DRIVER=build/src/frontend/ast_to_ir_test
sysycc=build/src/sysycc
testcase_dir="testcases/ast_to_ir"

if [ ! -f $DRIVER ];
then
    echo $DRIVER not found, please build it
    exit 1
fi

for x in `ls $testcase_dir/in | grep '.sy$'`;
do
    x=${x%.sy}
    fin=$testcase_dir/in/$x.sy
    fout=$testcase_dir/out/$x.ll
    tmp_ir_file=/tmp/ir-$RANDOM.ll
    echo check $fin $fout
    cmd="$sysycc --emit-ir --ir $tmp_ir_file $fin"
    $cmd
    if [ $? -ne 0 ]
    then
        echo fail to exec $cmd
        exit 1
    fi
    diff --color=always -b -B $fout $tmp_ir_file
    res=$?
    if [ $res -ne 0 ];
    then
        echo check ir here $tmp_ir_file
        echo 
        cat $tmp_ir_file
        exit 1
    else
        rm $tmp_ir_file
    fi
    tmp_out=/tmp/out-$RANDOM
    cmd="$sysycc --clang -o $tmp_out $fin -L libsysy"
    $cmd
    if [ $? -ne 0 ]
    then
        exit $?
    fi
    rm $tmp_out
done