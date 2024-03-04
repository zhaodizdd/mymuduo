cmake_minimum_required (VERSION 2.8)
project(mymuduo)

# mymuduo最终编译成so动态库， 设置动态库的理解， 放在当前目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/lib/")
# 设置调试信息 以及启动C++11语音标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 定义参与编译的源代码文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})
