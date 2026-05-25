cmake_minimum_required(VERSION 3.25)
project(ceramic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Git)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

include(CheckSymbolExists)
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

    set(LLVM_INCLUDEDIR "${LLVM_INCLUDE_DIRS}")
    set(LLVM_LIBDIR     "${LLVM_LIBRARY_DIRS}")
    separate_arguments(LLVM_CXXFLAGS UNIX_COMMAND "${LLVM_DEFINITIONS}")
    set(LLVM_LDFLAGS "")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND LLVM_CXXFLAGS -O0)
    endif()

elseif(WIN32)

    # TODO: unverified, port to find_package(LLVM CONFIG) + llvm_map_components_to_libnames.
    set(LLVM_DIR "$ENV{ProgramFiles}/LLVM" CACHE PATH "llvm install path")

    if(IS_DIRECTORY ${LLVM_DIR})
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}/share/llvm/cmake")
        include(LLVMConfig)

        include_directories(${LLVM_DIR}/include)
        link_directories(${LLVM_DIR}/lib)
        add_definitions(${LLVM_DEFINITIONS})

        llvm_map_components_to_libraries(LLVM_LIBS
            ${LLVM_TARGETS_TO_BUILD}
            jit interpreter native asmparser bitreader bitwriter ipo linker
        )

        find_file(CLANG clang.exe HINTS ${LLVM_DIR}/bin)
    else()
        message(FATAL_ERROR "Could not find LLVM. Try setting LLVM_DIR")
    endif()

    set(CMAKE_REQUIRED_INCLUDES ${LLVM_DIR}/include)
    set(CMAKE_REQUIRED_LIBRARIES ${LLVM_DIR}/lib/libclang.lib)

endif()

install(DIRECTORY doc/         DESTINATION share/doc/ceramic)
install(DIRECTORY examples/    DESTINATION share/doc/ceramic/examples)
install(DIRECTORY lib-ceramic  DESTINATION lib)

add_subdirectory(compiler)

if(EXISTS "${PROJECT_SOURCE_DIR}/tools")
    add_subdirectory(tools)
endif()

add_custom_target(test
    ${Python3_EXECUTABLE}
    "${PROJECT_SOURCE_DIR}/test/runtests.py"
    --buildflags="-Dtest.minimal"
)

add_dependencies(test ceramic)

