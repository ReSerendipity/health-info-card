cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 我们自己的「老人健康信息展示」代码抽成了根目录下的 health_display 组件（与框架/工具分离），
# 不在 components/ 也不在框架目录内。EXTRA_COMPONENT_DIRS 直接指向该组件目录本身，
# ESP-IDF 会把它作为一个外部组件加入构建（组件名取自目录名 health_display，与 main 的 REQUIRES 对应）。
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/health_display")

project(FoloToy-AI-Passport)
