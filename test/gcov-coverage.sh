
src_dir="/home/lt/projects/SysYCompiler/src"
build_dir="/home/lt/projects/SysYCompiler/build"
project="/home/lt/projects/SysYCompiler"


run_test() {
    cd $build_dir && ctest
    cd $project && $project/test/test_functional.sh
    cd $project && $project/test/test_ssa.sh
    cd $project && $project/test/test_irparser.sh
    cd $project && $project/test/test_backend.sh
    cd $project && $project/test/test_main.sh > /dev/null 2>&1
}
if [ "$1" = "test" ]; then
    run_test
    exit 0
fi

rm lcov.info > /dev/null 2>&1

for x in `find $build_dir | grep "\.gcda$"`; do
    if test -f $x; then
        echo $x
        lcov -d $x -c >> lcov.info
    fi
done

if [ ! -f lcov.info ]; then
    echo lcov.info not found
    exit 0
fi

lcov -r lcov.info '/usr/*' > tmp1.info
lcov -r tmp1.info '*json.hpp' > tmp2.info
lcov -r tmp2.info '*build/_deps/googletest-src/*' > tmp3.info
lcov -r tmp3.info '*/build/src/*' > lcov.info
rm tmp*.info

rm -r html
mkdir html && cd html && genhtml ../lcov.info

