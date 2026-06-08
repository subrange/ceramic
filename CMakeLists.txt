cmake_minimum_required(VERSION 3.25)
project(ceramic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Git)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

include(CPack)

if(UNIX)
    if(DEFINED LLVM_DIR AND EXISTS "${LLVM_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
        set(LLVM_DIR "${LLVM_DIR}/lib/cmake/llvm")
    endif()

    find_package(LLVM REQUIRED CONFIG)

    if(LLVM_PACKAGE_VERSION VERSION_LESS 19.0)
        message(FATAL_ERROR "LLVM 19 or newer is required. Found ${LLVM_PACKAGE_VERSION}")
    endif()

    message("-- Using LLVM ${LLVM_PACKAGE_VERSION}: ${LLVM_INSTALL_PREFIX}")
    message("-- LLVM include dir: ${LLVM_INCLUDE_DIRS}")

    if(LLVM_LINK_LLVM_DYLIB)
        set(LLVM_LIBS LLVM)
    else()
        llvm_map_components_to_libnames(LLVM_LIBS all)
    endif()

    separate_arguments(LLVM_CXXFLAGS UNIX_COMMAND "${LLVM_DEFINITIONS}")
endif()

install(DIRECTORY doc/         DESTINATION share/doc/ceramic)
install(DIRECTORY examples/    DESTINATION share/doc/ceramic/examples)
install(DIRECTORY lib-ceramic  DESTINATION lib)

add_subdirectory(compiler)
add_subdirectory(tools)

add_custom_target(test
    ${Python3_EXECUTABLE}
    "${PROJECT_SOURCE_DIR}/test/runtests.py"
    --buildflags="-Dtest.minimal"
)

add_dependencies(test ceramic)

