cmake_minimum_required(VERSION 3.10)

# 定义测试项目名称
project(Tests C)

# 获取所有模拟源文件
file(GLOB MOCK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/mocks/*.c")
# 明确排除 mock_ad5932_driver.c，因为它只用于API层测试
list(REMOVE_ITEM MOCK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_ad5932_driver.c")


# 添加协议层测试的可执行文件
add_executable(protocol_test test_protocol.c ${MOCK_SOURCES})
target_link_libraries(protocol_test PRIVATE protocol_layer)
target_include_directories(protocol_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
)


# 添加驱动层测试的可执行文件
# 修正：直接将ad5932_driver.c源文件添加到可执行文件中，而不是链接库
# 并且只使用 mock_spi.c，避免 mock_ad5932_driver.c 的冲突
add_executable(driver_test 
    test_driver.c 
    ${MOCK_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver/ad5932_driver.c
)
# 注意：我们不再链接 driver_layer 库
target_link_libraries(driver_test PRIVATE protocol_layer)
target_include_directories(driver_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver
)

# 添加API层测试的可执行文件
# 修正：将api层代码直接加入到可执行文件中
add_executable(api_test 
    test_api.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_ad5932_driver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/ad5932_api.c
)
# 注意：这里我们不再链接api_layer，直接包含源文件
target_link_libraries(api_test PRIVATE driver_layer)
target_include_directories(api_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_SDIR}/../api
)

# 将可执行文件添加到 CTest 测试套件中
add_test(NAME run_protocol_test COMMAND protocol_test)
add_test(NAME run_driver_test COMMAND driver_test)
add_test(NAME run_api_test COMMAND api_test)