#该测试脚本没有变动
sysycc="build/src/sysycc"
sy_file="testcases/ast_to_ir/in/005_while.sy"
ir_file="testcases/ast_to_ir/out/005_while.ll"

# help
$sysycc -h

# compile
$sysycc $sy_file

# emit-ir
$sysycc $sy_file --emit-ir --emit-asm
$sysycc $sy_file --emit-ir --ir /tmp/tmp.ll --emit-asm
$sysycc $sy_file --emit-ir --ir /tmp/tmp.ll -f main --emit-asm

# enable opt
$sysycc $sy_file --enable-all-opt --emit-asm
$sysycc $sy_file --opt mem2reg --emit-asm
$sysycc $sy_file -O mem2reg --emit-asm
$sysycc $sy_file -O mem2reg -O constant --emit-asm

# list opt
$sysycc $sy_file --list-opt --emit-asm

# use clang backend
$sysycc $sy_file --clang -L libsysy-arm/lib 
$sysycc $sy_file --clang --lib-dir libsysy-arm/lib

$sysycc $sy_file --out /tmp/a.out

# verbose
$sysycc $sy_file --opt mem2reg -v -v --emit-asm

# emit asm
$sysycc $sy_file --emit-asm
$sysycc $sy_file --emit-asm --asm /tmp/tmp.s

# as
$sysycc $sy_file --as arm-linux-gnueabi-as --emit-asm

# invalid input
$sysycc file_not_found
$sysycc $sy_file --emit-ir --ir /file_not_writable
