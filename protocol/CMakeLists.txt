add_library(protocol_layer STATIC
  spi_hal.c
)
target_include_directories(protocol_layer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})