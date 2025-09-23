add_library(api_layer STATIC
    ad5932_api.c
)
target_include_directories(api_layer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(api_layer PRIVATE driver_layer)