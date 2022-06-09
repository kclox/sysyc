
build_dir="/home/qihan/sysyc_Tsihan/build"

for x in `find $build_dir | grep -E "(\.gcda$|\.gcno$)"`; do
    if test -f $x; then
        echo $x
        rm $x
    fi
done