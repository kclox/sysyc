#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

sysycc=build/src/sysycc

tmp_dir=/tmp/test_ir
mkdir -p $tmp_dir

pass_count=0
fail_count=0
total=`find -L testcases/ | grep "\.sy$" | grep function | wc -l`

print_process() {
    echo "pass/fail/total: $pass_count/$fail_count/$total"
}

failed() {
    err=$1
    msg=$2
    notquit=$3
    echo -e "fail: $err"
    if [ ! -z "$msg" ]
    then
        echo -e "$msg"
    fi
    fail_count=$(( $fail_count + 1 ))
    print_process
    [ -z "$notquit" ] && exit 1
}

for x in `find -L testcases/ | grep "\.sy$" | grep function`;
do
    file=`basename $x`
    file=${file%.sy}
    out=/tmp/tmp.out
    echo -e "source: >>>\t$x"
    compile="$sysycc --clang -L libsysy -o $out $x --enable-all-opt"
    compile_out=`$compile 2>&1`
    if [ $? -ne 0 ]
    then
        failed "$compile" "$compile_out" 1
        continue
    fi
    input_dir=`dirname $x`
    input=$input_dir/$file.in
    expected=$input_dir/$file.out
    res=/tmp/tmp.res
    if [ -f $input ]
    then
        echo -e "input:\t\t$input"
        $out >$res 2>/tmp/tmp.err < $input
        echo -e "\n$?" >> $res
    else
        $out >$res 2>/tmp/tmp.err
        echo -e "\n$?" >> $res
    fi
    if [ -f $expected ]
    then
        echo -e "output:\t\t$expected"
        cmd="diff -B -b --color=always $res $expected"
        $cmd
        if [ $? -ne 0 ]
        then
            failed "$cmd" "compile: $compile" 1
            continue
        fi
    else
        echo -e "output:\t\tskipped"
    fi
    pass_count=$(( $pass_count + 1 ))
done

print_process
