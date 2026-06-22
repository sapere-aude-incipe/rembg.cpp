cmake_minimum_required(VERSION 3.14)
project(rembg.cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(ONNXRUNTIME_ROOT "" CACHE PATH "Path to a prebuilt ONNX Runtime package")
if(NOT ONNXRUNTIME_ROOT)
    message(FATAL_ERROR "Set ONNXRUNTIME_ROOT to a prebuilt ONNX Runtime package directory")
endif()

if(NOT EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found under ${ONNXRUNTIME_ROOT}/include")
endif()

if(NOT EXISTS "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib" AND WIN32)
    message(FATAL_ERROR "onnxruntime.lib not found under ${ONNXRUNTIME_ROOT}/lib")
endif()

add_library(rembg STATIC rembg.cpp rembg.h)
add_library(rembg::rembg ALIAS rembg)

target_include_directories(rembg
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
        ${ONNXRUNTIME_ROOT}/include
)

target_link_directories(rembg PUBLIC "${ONNXRUNTIME_ROOT}/lib")
target_link_libraries(rembg PUBLIC onnxruntime)
target_compile_features(rembg PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(rembg PRIVATE /W3 /EHsc /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(rembg PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(rembg_copy_onnxruntime target_name)
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_shared.dll"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Copying ONNX Runtime DLLs")
    endif()
endfunction()

option(REMBG_BUILD_EXAMPLES "Build example executables" ON)
if(REMBG_BUILD_EXAMPLES AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(examples)
endif()
