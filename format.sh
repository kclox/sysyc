#!/bin/bash

c=`find src/ | grep "\.c$"`
cc=`find src/ | grep "\.cc$"`
cpp=`find src/ | grep "\.cpp$"`
h=`find src/ | grep "\.h$"`
hpp=`find src/ | grep "\.hpp$"`

all=$c" "$cc" "$cpp" "$h" "$hpp

for x in $all
do
    if [[ $x = *.tab.* ]] || [[ $x = *.lex.* ]]
    then
        continue
    fi
    echo formatting $x
    clang-format -style=.clang-format -i $x
done
