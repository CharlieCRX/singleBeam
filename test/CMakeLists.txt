cmake_minimum_required(VERSION 3.10)

# 定义测试项目名称
project(Tests C)

# 获取所有模拟源文件
file(GLOB MOCK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/mocks/*.c")

# 添加驱动层测试的可执行文件
add_executable(driver_test test_driver.c ${MOCK_SOURCES})
target_link_libraries(driver_test PRIVATE driver_layer)
target_include_directories(driver_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver
)

# 添加协议层测试的可执行文件
add_executable(protocol_test test_protocol.c ${MOCK_SOURCES})
target_link_libraries(protocol_test PRIVATE protocol_layer)
target_include_directories(protocol_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
)

add_test(NAME run_driver_test COMMAND driver_test)


# 将 mocks 和 protocol 目录设置为头文件搜索路径
# 这样就可以找到 mock_spi.h 和 spi_hal.h 等头文件
target_include_directories(protocol_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
)

# 将可执行文件添加到 CTest 测试套件中
add_test(NAME run_protocol_test COMMAND protocol_test)