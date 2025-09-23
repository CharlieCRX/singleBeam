add_library(driver_layer STATIC
    ad5932_driver.c
)
target_include_directories(driver_layer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(driver_layer PRIVATE protocol_layer)