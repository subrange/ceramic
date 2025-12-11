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

set(CLAY_SOURCES
    clay.cpp
)

set(CLAYDOC_SOURCES
    claydoc.cpp
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

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CLAY_CXXFLAGS "${LLVM_CXXFLAGS} -UNDEBUG")
else()
    set(CLAY_CXXFLAGS "${LLVM_CXXFLAGS} -DNDEBUG")
endif()
if (NOT MSVC)
    set(CLAY_CXXFLAGS "${CLAY_CXXFLAGS} -fexceptions")
endif()

set(ENABLE_PCH False CACHE BOOL
    "Use precompiled headers when building the compiler. (experimental)")

if(ENABLE_PCH)
    precompile_header(clay.hpp SOURCES pch.cpp "${CLAY_CXXFLAGS}")
endif(ENABLE_PCH)

# Targets
add_library(compiler STATIC ${COMPILER_SOURCES})
add_executable(clay ${CLAY_SOURCES})
add_executable(claydoc ${CLAYDOC_SOURCES})
add_executable(ut ${UT_SOURCES})

# compiler flags
target_compile_options(compiler PRIVATE "${CLAY_CXXFLAGS}")
target_compile_options(clay     PRIVATE "${CLAY_CXXFLAGS}")
target_compile_options(claydoc  PRIVATE "${CLAY_CXXFLAGS}")
target_compile_options(ut       PRIVATE "${CLAY_CXXFLAGS}")

if (UNIX)
    target_link_options(compiler PRIVATE "${LLVM_LDFLAGS}")
    target_link_options(clay     PRIVATE "${LLVM_LDFLAGS}")
    target_link_options(claydoc  PRIVATE "${LLVM_LDFLAGS}")
    target_link_options(ut       PRIVATE "${LLVM_LDFLAGS}")
endif(UNIX)

# Libraries
target_link_libraries(clay     PRIVATE compiler ${LLVM_LIBS})
target_link_libraries(claydoc  PRIVATE compiler ${LLVM_LIBS})
target_link_libraries(ut       PRIVATE compiler ${LLVM_LIBS})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(clay     PRIVATE rt dl pthread)
    target_link_libraries(claydoc  PRIVATE rt dl pthread)
    target_link_libraries(ut       PRIVATE pthread)
endif()

# Install
install(TARGETS clay RUNTIME DESTINATION bin)
install(TARGETS claydoc RUNTIME DESTINATION bin)
