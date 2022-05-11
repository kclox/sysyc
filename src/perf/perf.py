
from doctest import REPORTING_FLAGS
import json
import os
import re
import time

sysycc = "build/src/sysycc"
clang = "clang"
testcase_dir = "./testcases/official-testcases/2021_qual/performance"
testcases = {}
lib = "libsysy"


def find_testcases(testcase_dir):
    testcases = {}
    p = os.walk(testcase_dir)
    for _, _, files in p:
        for f in files:
            t = re.findall(r'([a-z]+)\d\.sy', f)
            if len(t) > 0:
                ty = t[0]
                if ty not in testcases:
                    testcases[ty] = []
                testcases[ty].append(f)
    return testcases


def get_testcase_input(t):
    return t[:-2] + "in"


def get_testcase_output(t):
    return t[:-2] + "out"


def get_compile_arg(t):
    return os.path.join(testcase_dir, t)


def run_sysycc_no_backend(t):
    before = time.perf_counter()
    os.system('%s --disable-backend %s --emit-ir --ir tmp.s' % (sysycc, t))
    after = time.perf_counter()
    t = after - before
    return t


def run_clang_no_backend(t):
    os.system("cp '%s' tmp.c" % t)
    before = time.perf_counter()
    os.system(
        '%s tmp.c -c -Xclang -disable-O0-optnone -O0 -S -emit-llvm >/dev/null 2>&1' % clang)
    after = time.perf_counter()
    t = after - before
    return t


def run_sysycc(t, opt=''):
    before = time.perf_counter()
    os.system('%s %s --clang -L %s %s --emit-ir --ir tmp.ll >/dev/null 2>&1' %
              (sysycc, t, lib, opt))
    after = time.perf_counter()
    t = after - before
    return t


def perf(f):
    before = time.perf_counter()
    f()
    after = time.perf_counter()
    return after - before


def run_clang(t, opt=''):
    os.system("cp '%s' tmp.c" % t)
    before = time.perf_counter()
    os.system(
        "%s -m32 -c -Xclang -disable-O0-optnone -O0 -S -emit-llvm tmp.c >/dev/null 2>&1" % clang)
    os.system("opt tmp.ll %s -S > tmp.opt.ll" % opt)
    os.system('%s -m32 -O0 tmp.opt.ll %s >/dev/null 2>&1' %
              (clang, lib + "/libsysy.a"))
    after = time.perf_counter()

    def f():
        os.system("opt tmp.ll -S >/dev/null 2>&1")
    t = after - before - perf(f)
    return t


def run_sysycc_perf(t):
    os.system(
        "%s %s --enable-all-opt --disable-backend --emit-ir --ir tmp.ll" % (sysycc, t))
    os.system(
        'clang-10 -target thumb-pc-linux-gnu -O0 tmp.ll -S -no-integrated-as -m32 >/dev/null 2>&1')
    os.system('arm-linux-gnueabi-as tmp.s -o tmp.o')
    os.system('arm-linux-gnueabi-gcc tmp.o libsysy/libsysy-arm.a -static')
    program = "./a.out"

    def f():
        os.system("timeout 30 %s < %s  > /dev/null 2>&1" %
                  (program, get_testcase_input(t)))
    return perf(f)


def run_clang_perf(t, opt='-O0'):
    os.system("cp '%s' tmp.c" % t)
    os.system(
        'clang-10 tmp.c -target thumb-pc-linux-gnu %s -S -no-integrated-as -m32 >/dev/null 2>&1' % (opt))
    os.system('arm-linux-gnueabi-as tmp.s -o tmp.o')
    os.system('arm-linux-gnueabi-gcc tmp.o libsysy/libsysy-arm.a -static')
    program = "./a.out"

    def f():
        os.system("timeout 30 %s < %s > /dev/null 2>&1" %
                  (program, get_testcase_input(t)))
    return perf(f)

# frontend, normal, opt, O3
# opt, O3


testcases = find_testcases(testcase_dir)
sysycc_compile_perf = [{} for i in range(3)]
clang_compile_perf = [{} for i in range(4)]
sysycc_perf = [{}]
clang_perf = [{}, {}]
for k, v in testcases.items():
    print(k)
    for i in range(3):
        sysycc_compile_perf[i][k] = []
        clang_compile_perf[i][k] = []
    clang_compile_perf[3][k] = []
    sysycc_perf[0][k] = []
    clang_perf[0][k], clang_perf[1][k] = [], []
    for t in v:
        file = get_compile_arg(t)
        # compile perf
        # frontend
        p1 = run_sysycc_no_backend(file)
        p2 = run_clang_no_backend(file)
        print("frontend\t", p1, p2)
        sysycc_compile_perf[0][k].append(p1)
        clang_compile_perf[0][k].append(p2)
        # normal
        p1 = run_sysycc(file)
        p2 = run_clang(file, "-O0")
        print("normal\t\t", p1, p2)
        sysycc_compile_perf[1][k].append(p1)
        clang_compile_perf[1][k].append(p2)
        # opt
        p1 = run_sysycc(file, "-O mem2reg -O constant")
        p2 = run_clang(file, "-mem2reg")
        print("opt\t\t", p1, p2)
        sysycc_compile_perf[2][k].append(p1)
        clang_compile_perf[2][k].append(p2)
        # O3
        p2 = run_clang(file, "-O3")
        print("O3\t\t", p2)
        clang_compile_perf[3][k].append(p2)
        # print(get_testcase_input(t))
        # print(get_testcase_output(t))

        p1 = run_sysycc_perf(file)
        if p1 > 30:
            print("blacklist", t)
            continue
        p2 = run_clang_perf(file)
        p3 = run_clang_perf(file, '-O3')
        print(p1, p2, p3)
        sysycc_perf[0][k].append(p1)
        clang_perf[0][k].append(p2)
        clang_perf[1][k].append(p3)

with open("testcases.json", "w") as fp:
    json.dump(testcases, fp, indent=2)

with open("sysycc.compile.json", "w") as fp:
    json.dump(sysycc_compile_perf, fp, indent=2)

with open("clang.compile.json", "w") as fp:
    json.dump(clang_compile_perf, fp, indent=2)

with open("sysycc.perf.json", "w") as fp:
    json.dump(sysycc_perf, fp, indent=2)

with open("clang.perf.json", "w") as fp:
    json.dump(clang_perf, fp, indent=2)
