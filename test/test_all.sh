#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

sysycc=build/src/sysycc

tmp_dir=/tmp/test_ir
mkdir -p $tmp_dir

#将失败用例的记录存放起来
save_dir=testlogs/test_all
mkdir -p $save_dir
time=$(date "+%Y%m%d-%H%M%S")
touch "${save_dir}/${time}.log"
echo "the running time is ${time}" 

pass_count=0
fail_count=0
total=`find -L testcases/official-testcases/ | grep "\.sy$" | wc -l`

print_process() {
    echo "pass/fail/total: $pass_count/$fail_count/$total"
}

failed() {
    err=$1
    msg=$2
    notquit=$3
    echo -e "fail: $err">>${save_dir}/${time}.log
    if [ ! -z "$msg" ]
    then
        echo -e "$msg\n\n" >>${save_dir}/${time}.log
    fi
    fail_count=$(( $fail_count + 1 ))
    print_process
    #尝试将失败的测试用例保存在文件中

    [ -z "$notquit" ] && exit 1
}

for x in `find -L testcases/official-testcases/ | grep "\.sy$"`;
do
    file=`basename $x`
    file=${file%.sy}
    out=/tmp/tmp.out
    echo -e "source: >>>\t$x"
    compile="$sysycc --clang -L libsysy -o $out $x"
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
        timeout 5 $out >$res 2>/tmp/tmp.err < $input
        echo -e "\n$?" >> $res
    else
        timeout 5 $out >$res 2>/tmp/tmp.err
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
