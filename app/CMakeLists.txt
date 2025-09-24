# app/CMakeLists.txt
add_library(app_layer STATIC
  wavegen_app.c
)

target_include_directories(app_layer PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../api
)
# 不再在此处链接 api_layer，避免与测试中的mock冲突