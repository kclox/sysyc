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
