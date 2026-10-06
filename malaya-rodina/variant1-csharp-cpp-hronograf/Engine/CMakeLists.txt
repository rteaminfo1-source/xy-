# Сборка C++ движка через CMake (Visual Studio «Открыть папку», CLion, Linux/macOS).
cmake_minimum_required(VERSION 3.15)
project(hronograf_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(hronograf_engine SHARED engine.cpp engine.h)

if(MSVC)
  target_compile_options(hronograf_engine PRIVATE /O2 /utf-8 /W3)
  set_property(TARGET hronograf_engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
  target_compile_options(hronograf_engine PRIVATE -O2 -Wall -Wextra)
  find_package(Threads REQUIRED)
  target_link_libraries(hronograf_engine PRIVATE Threads::Threads)
  if(WIN32)
    target_link_options(hronograf_engine PRIVATE -static)
  endif()
endif()
