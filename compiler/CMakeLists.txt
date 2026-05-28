include(PCHsupport)

set(COMPILER_SOURCES
    analyzer.cpp
    analyzer_op.cpp
    clone.cpp
    codegen.cpp
    codegen_op.cpp
    constructors.cpp
    desugar.cpp
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

# compiler flags
target_compile_options(compiler   PRIVATE ${LLVM_CXXFLAGS})
target_compile_options(ceramic    PRIVATE ${LLVM_CXXFLAGS})
target_compile_options(ceramicdoc PRIVATE ${LLVM_CXXFLAGS})
target_compile_options(ut         PRIVATE ${LLVM_CXXFLAGS})

# include directories
target_include_directories(compiler   PRIVATE ${LLVM_INCLUDEDIR})
target_include_directories(ceramic    PRIVATE ${LLVM_INCLUDEDIR})
target_include_directories(ceramicdoc PRIVATE ${LLVM_INCLUDEDIR})
target_include_directories(ut         PRIVATE ${LLVM_INCLUDEDIR})

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(compiler   PRIVATE UNDEBUG)
    target_compile_definitions(ceramic    PRIVATE UNDEBUG)
    target_compile_definitions(ceramicdoc PRIVATE UNDEBUG)
    target_compile_definitions(ut         PRIVATE UNDEBUG)
else()
    target_compile_definitions(compiler   PRIVATE NDEBUG)
    target_compile_definitions(ceramic    PRIVATE NDEBUG)
    target_compile_definitions(ceramicdoc PRIVATE NDEBUG)
    target_compile_definitions(ut         PRIVATE NDEBUG)
endif()

if(NOT MSVC)
    target_compile_options(compiler   PRIVATE -fexceptions)
    target_compile_options(ceramic    PRIVATE -fexceptions)
    target_compile_options(ceramicdoc PRIVATE -fexceptions)
    target_compile_options(ut         PRIVATE -fexceptions)
endif()

set(ENABLE_PCH False CACHE BOOL
        "Use precompiled headers when building the compiler. (experimental)")

if(ENABLE_PCH)
    precompile_header(ceramic.hpp SOURCES pch.cpp ${LLVM_CXXFLAGS})
endif(ENABLE_PCH)

if (UNIX)
    target_link_options(compiler   PRIVATE ${LLVM_LDFLAGS})
    target_link_options(ceramic    PRIVATE ${LLVM_LDFLAGS})
    target_link_options(ceramicdoc PRIVATE ${LLVM_LDFLAGS})
    target_link_options(ut         PRIVATE ${LLVM_LDFLAGS})
endif()

# Libraries
target_link_libraries(ceramic    PRIVATE compiler ${LLVM_LIBS})
target_link_libraries(ceramicdoc PRIVATE compiler ${LLVM_LIBS})
target_link_libraries(ut         PRIVATE compiler ${LLVM_LIBS})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ceramic    PRIVATE rt dl pthread)
    target_link_libraries(ceramicdoc PRIVATE rt dl pthread)
    target_link_libraries(ut         PRIVATE pthread)
endif()

# Install
install(TARGETS ceramic RUNTIME DESTINATION bin)
install(TARGETS ceramicdoc RUNTIME DESTINATION bin)
