cmake_minimum_required(VERSION 3.10)
project(qtasyncfuture VERSION 1.0.0 LANGUAGES CXX)

add_library(qtasyncfuture INTERFACE)
target_include_directories(qtasyncfuture INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()

set_target_properties(qtasyncfuture PROPERTIES PUBLIC_HEADER "asyncfuture.h")


install(TARGETS qtasyncfuture
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include)

export(TARGETS qtasyncfuture NAMESPACE qtasyncfuture:: FILE qtasyncfutureConfig.cmake)
set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)
export(PACKAGE qtasyncfuture)
