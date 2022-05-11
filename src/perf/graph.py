import matplotlib.pyplot as plt
import numpy as np
import json

testcases = {}
sysycc_compile_perf = {}
clang_compile_perf = {}
sysycc_perf = {}
clang_perf = {}


def load_data():
    global testcases, sysycc_compile_perf, clang_compile_perf, sysycc_perf, clang_perf
    with open("testcases.json") as fp:
        testcases = json.load(fp)

    with open("sysycc.compile.json") as fp:
        sysycc_compile_perf = json.load(fp)

    with open("clang.compile.json") as fp:
        clang_compile_perf = json.load(fp)

    with open("sysycc.perf.json") as fp:
        sysycc_perf = json.load(fp)

    with open("clang.perf.json") as fp:
        clang_perf = json.load(fp)


def calc_group_perf(arr):
    for i, d in enumerate(arr):
        for k, v in d.items():
            s = 0
            for x in v:
                s += x
            arr[i][k] = s
    return arr


def get_data(arr):
    print(arr)
    res = []
    for d in arr:
        res.append([])
        for k in order:
            res[-1].append(d[k])
    return res


load_data()
order = testcases.keys()

sysycc_compile_data = get_data(calc_group_perf(sysycc_compile_perf))
clang_compile_data = get_data(calc_group_perf(clang_compile_perf))
sysycc_perf_data = get_data(calc_group_perf(sysycc_perf))
clang_perf_data = get_data(calc_group_perf(clang_perf))

# [0: frontend, 1: normal, 2: opt, 3: O3]
# {"type": [data]}
# draw
xn = len(testcases)
x = np.arange(xn)

total_width, n = 0.8, 2
width = total_width / n
# x = x - (total_width - width) / 2
# print(x)

#
print(sysycc_compile_data[0], clang_compile_data[0])
plt.ylabel("time (s)")
plt.xticks(x, list(order))
plt.bar(x - width/2, sysycc_compile_data[0],
        width=width, label="sysycc(no backend)")
plt.bar(x + width/2, clang_compile_data[0],
        width=width, label="clang(no backend)")
plt.legend()
plt.show()
plt.clf()

# normal
print(sysycc_compile_data[1], clang_compile_data[1])
plt.ylabel("time (s)")
plt.xticks(x, list(order))
plt.bar(x - width/2, sysycc_compile_data[1], width=width, label="sysycc")
plt.bar(x + width/2, clang_compile_data[1],
        width=width, label="clang")
plt.legend()
plt.show()
plt.clf()

total_width, n = 0.8, 3
width = total_width / n

# opt
print(sysycc_compile_data[2], clang_compile_data[2], clang_compile_data[3])
plt.ylabel("time (s)")
plt.xticks(x, list(order))
plt.bar(x - width, sysycc_compile_data[2], width=width, label="sysycc")
plt.bar(x, clang_compile_data[2],
        width=width, label="clang")
plt.bar(x + width, clang_compile_data[3],
        width=width, label="clang -O3")
plt.legend()
plt.show()
plt.clf()

# perf
plt.ylabel("time (s)")
plt.xticks(x, list(order))
plt.bar(x - width, sysycc_perf_data[0], width=width, label="sysycc")
plt.bar(x, clang_perf_data[0],
        width=width, label="clang")
plt.bar(x + width, clang_perf_data[1],
        width=width, label="clang -O3")
plt.legend()
plt.show()
plt.clf()


a = []
b = []
o = []
for i in range(5):
    if i == 1 or i == 2:
        continue
    a.append(sysycc_perf_data[0][i])
    b.append(clang_perf_data[0][i])
    o.append(list(order)[i])

total_width, n = 0.8, 2
width = total_width / n

x = np.arange(3)
plt.ylabel("time (s)")
plt.xticks(x, o)
plt.bar(x - width/2, a, width=width, label="sysycc")
plt.bar(x + width / 2, b,
        width=width, label="clang")
plt.legend()
plt.show()
plt.clf()
