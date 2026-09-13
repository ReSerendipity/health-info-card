cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 我们自己的「老人健康信息展示」代码抽成了根目录下的 health-display 组件，
# 不在工具/框架目录内，这里把它加入组件搜索路径。
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/health-display")

project(FoloToy-AI-Passport)
