cmake_minimum_required(VERSION 3.10)
project(AD5932_Library_Test C)

# 启用测试功能
enable_testing()

add_subdirectory(protocol)
add_subdirectory(driver)
add_subdirectory(api)
add_subdirectory(test)