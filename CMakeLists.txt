# 设置最低 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(CacheStrategy)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 强制使用C++17标准。
# 如果编译器不支持C++17，则CMake构建会失败。
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 指定源文件目录下的所有 .cpp 文件
# 使用file(GLOB ...) 指令自动搜索当前目录下的所有.cpp文件。
# 搜索到的文件列表被存储在变量SOURCES中。
file(GLOB SOURCES "*.cpp")

# 设置目标可执行文件
# 告诉CMake用上一步搜索到的所有源文件编译并生成一个名为main的可执行程序。
add_executable(main ${SOURCES})

# 清理中间的 .o 文件
set_target_properties(main PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# 额外的编译选项（可根据需要启用）
# target_compile_options(main PRIVATE -Wall -Wextra -O2)