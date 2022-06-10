#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

DRIVER=build/src/frontend/parse_ast_tool
testcase_dir="testcases/official-testcases"

#将失败用例的记录存放起来 这里还待解决的问题是yyerror的捕获
save_dir=testlogs/test_ast
mkdir -p $save_dir
time=$(date "+%Y%m%d-%H%M%S")
touch "${save_dir}/${time}.log"
echo "the running time is ${time}" 

if [ ! -f $DRIVER ];
then
    echo $DRIVER not found, please build it
    exit 1
fi

i=0
for x in `find $testcase_dir | grep '\.sy$'`;
do
    i=$((i + 1))
    echo $i $x
    timeout 3s $DRIVER $x > /dev/null
    res=$?
    if [[ $res -ne 0 ]];
    then
        echo $res
        #echo -e $res >>${save_dir}/${time}.log
        echo $DRIVER $x
        if [[ $res -ne 124 ]];
        then
            exit 1
        fi
        echo "timeout"
        #记录超时
        
        echo -e $res >>${save_dir}/${time}.log
        echo -e $DRIVER $x >>${save_dir}/${time}.log
        echo -e "timeout\n\n" >>${save_dir}/${time}.log

    fi
done