#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

DRIVER=build/src/frontend/parse_ast_tool
testcase_dir="testcases/official-testcases"

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
        echo $DRIVER $x
        if [[ $res -ne 124 ]];
        then
            exit 1
        fi
        echo "timeout"
    fi
done