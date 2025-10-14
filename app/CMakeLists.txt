# app/CMakeLists.txt
add_executable(ad5932_main
    ad5932_main.c
)

# 链接依赖的静态库（来自其他模块）
target_link_libraries(ad5932_main
    PRIVATE
    api_layer
    driver_layer
    protocol_layer
)

# 包含头文件路径（如果这些模块没有全局 install）
target_include_directories(ad5932_main PRIVATE
    ${CMAKE_SOURCE_DIR}/api
    ${CMAKE_SOURCE_DIR}/driver
    ${CMAKE_SOURCE_DIR}/protocol
    ${CMAKE_SOURCE_DIR}/common
)

# 如果使用 usleep / getopt_long 等函数，需要定义 POSIX 宏
target_compile_definitions(ad5932_main PRIVATE _POSIX_C_SOURCE=200809L)
