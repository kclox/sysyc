#!/bin/bash

source test/comm.sh
[ $? -ne 0 ] && exit

cloc src --force-lang=yacc,yy --exclude-list-file=test/exclude.list
