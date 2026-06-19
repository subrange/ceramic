set(COMPILER_SOURCES
    analyzer.cpp
    analyzer_op.cpp
    clone.cpp
    codegen.cpp
    codegen_op.cpp
    constructors.cpp
    desugar.cpp
    diagnostic.cpp
    env.cpp
    error.cpp
    evaluator.cpp
    evaluator_op.cpp
    externals.cpp
    hirestimer.cpp
    interactive.cpp
    invoketables.cpp
    lambdas.cpp
    lexer.cpp
    literals.cpp
    loader.cpp
    matchinvoke.cpp
    objects.cpp
    parachute.cpp
    parser.cpp
    patterns.cpp
    printer.cpp
    profiler.cpp
    types.cpp
)

set(CERAMIC_SOURCES
    ceramic.cpp
)

set(CERAMICDOC_SOURCES
    ceramicdoc.cpp
    html.cpp
)

set_property(SOURCE ceramicdoc.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS "STYLE_CSS_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/style.css\"")

set(UT_SOURCES
    refcounted_ut.cpp
    ut_main.cpp
)

# Git Commit ID
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_WC_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    set_property(SOURCE main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS "GIT_ID=\"${GIT_WC_ID}\"")
endif()

# Targets
add_library(compiler STATIC ${COMPILER_SOURCES})
add_executable(ceramic ${CERAMIC_SOURCES})
add_executable(ceramicdoc ${CERAMICDOC_SOURCES})
add_executable(ut ${UT_SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(_debug_def UNDEBUG)
else()
    set(_debug_def NDEBUG)
endif()

foreach(tgt IN ITEMS compiler ceramic ceramicdoc ut)
    target_compile_options(${tgt} PRIVATE ${LLVM_CXXFLAGS})
    target_include_directories(${tgt} PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(${tgt} PRIVATE ${_debug_def})
    if(NOT MSVC)
        target_compile_options(${tgt} PRIVATE -fexceptions
            -Wall -Wextra -Wconversion -Wdouble-promotion
            -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion)
    endif()
endforeach()

# Libraries
target_link_libraries(ceramic    PRIVATE compiler ${LLVM_LIBS})
target_link_libraries(ceramicdoc PRIVATE compiler ${LLVM_LIBS})
target_link_libraries(ut         PRIVATE compiler ${LLVM_LIBS})

find_package(Threads REQUIRED)
target_link_libraries(ceramic    PRIVATE Threads::Threads)
target_link_libraries(ceramicdoc PRIVATE Threads::Threads)
target_link_libraries(ut         PRIVATE Threads::Threads)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ceramic    PRIVATE rt dl)
    target_link_libraries(ceramicdoc PRIVATE rt dl)
endif()

# Install
install(TARGETS ceramic RUNTIME DESTINATION bin)
install(TARGETS ceramicdoc RUNTIME DESTINATION bin)
