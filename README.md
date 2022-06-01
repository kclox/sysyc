# SysYCompiler

## 项目结构

- src 源代码
    - include 头文件目录，方便被各个模块引用
    - comm 公共部分（基本类型，实用函数等等）
    - frontend 前端部分（包括 AST 转 IR）
    - opt 优化部分
    - backend 后端部分
- test 测试脚本
- testcases 测试样例
- libsysy* 官方提供的运行库
- examples 例子


## 开发指南

### 环境

推荐 Ubuntu 20.04

### 构建

#### 依赖

- CMake (>= 3.16.x)
- make
- g++
- bison
- flex

Ubuntu 20.04 下直接运行以下命令即可

```shell
sudo apt install -y cmake make g++ bison flex
```

#### 编译

```shell
mkdir build
cd build
cmake ..
make
```

### 开发

#### 依赖

- git
- clang-format (>= 10.0.x)

Ubuntu 20.04 下可以直接使用以下命令安装

```shell
sudo apt install -y git clang-format
```

## 使用

- 生成中间代码

```shell
sysycc --emit-ir [file]
```

- 优化

```shell
# 开启所有优化
sysycc --emit-ir --enable-all-opt [file]
# 指定优化
sysycc --emit-ir --opt [opt name] [file]
# 例：
sysycc --emit-ir --opt mem2reg --opt constant [file]
```

- 生成汇编

```shell
sysycc --emit-asm [file]
```


-- debug
```
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

- 在虚拟机下执行 ARM
```
all:
	arm-linux-gnueabihf-as xxx.s -g -o xxxx.o
	arm-linux-gnueabihf-gcc xxxx.o  -static libsysy-arm/lib/libsysy.a -o xxxx

qemu-arm ./asm32
```

- 在虚拟机下调试 ARM
```
qemu-arm -L /usr/arm-linux-gnueabihf -g 1234 ./xxxx  // 1234为远程端口
gdb-multiarch -q --nh -ex 'set architecture arm' -ex 'file xxxx' -ex 'target remote localhost:1234' -ex 'layout split' -ex 'layout regs'

```

