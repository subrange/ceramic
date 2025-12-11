cmake_minimum_required(VERSION 3.25)
project(clay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Git)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

include(CheckSymbolExists)
include(CPack)

if(UNIX)
    if(DEFINED LLVM_DIR)
        set(LLVM_CONFIG "${LLVM_DIR}/bin/llvm-config")
    else()
        find_program(LLVM_CONFIG llvm-config /usr/local/bin DOC "path to llvm-config")
    endif()

    message("-- Using llvm-config: ${LLVM_CONFIG}")

    if(NOT LLVM_CONFIG)
        message(FATAL_ERROR "llvm-config not found. Install LLVM.")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --version
        OUTPUT_VARIABLE LLVM_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LLVM_CONFIG_VERSION_RESULT
    )
    if (NOT ${LLVM_CONFIG_VERSION_RESULT} EQUAL 0)
        message(FATAL_ERROR "${LLVM_CONFIG} failed")
    endif()
    if(LLVM_VERSION VERSION_LESS 18.0)
        message(FATAL_ERROR "LLVM 18 or newer is required. Found ${LLVM_VERSION}")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --cxxflags
        OUTPUT_VARIABLE LLVM_CXXFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS}")
    separate_arguments(LLVM_CXXFLAGS)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LLVM_CXXFLAGS "${LLVM_CXXFLAGS} -O0")
    endif()

    execute_process(
        COMMAND ${LLVM_CONFIG} --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    separate_arguments(LLVM_LDFLAGS)

    execute_process(
        COMMAND ${LLVM_CONFIG} --libs all
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    separate_arguments(LLVM_LIBS)

    execute_process(
        COMMAND ${LLVM_CONFIG} --libdir
        OUTPUT_VARIABLE LLVM_LIBDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${LLVM_CONFIG} --prefix
        OUTPUT_VARIABLE LLVM_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${LLVM_CONFIG} --includedir
        OUTPUT_VARIABLE LLVM_INCLUDEDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LLVM_INCLUDEDIR "${LLVM_INCLUDEDIR}")

    message("-- Using LLVM: ${LLVM_DIR}")
    message("-- LLVM include dir: ${LLVM_INCLUDEDIR}")

    set(CMAKE_REQUIRED_INCLUDES ${LLVM_DIR}/include)
    set(CMAKE_REQUIRED_LIBRARIES clang)
    set(CMAKE_REQUIRED_FLAGS "-L${LLVM_LIBDIR}")

elseif(WIN32)

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

set(BUILD_BINDGEN True CACHE BOOL
    "Build the clay-bindgen tool for generating Clay bindings from C header files."
)

install(DIRECTORY doc/        DESTINATION share/doc/clay)
install(DIRECTORY examples/   DESTINATION share/doc/clay/examples)
install(DIRECTORY lib-clay    DESTINATION lib)

add_subdirectory(compiler)

if(EXISTS "${PROJECT_SOURCE_DIR}/tools")
    add_subdirectory(tools)
endif()

add_custom_target(test
    ${Python3_EXECUTABLE}
    "${PROJECT_SOURCE_DIR}/test/runtests.py"
    --buildflags="-Dtest.minimal"
)

add_dependencies(test clay)

