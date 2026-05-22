set(BINDGEN_SOURCES
    bindgen.ceramic
)

if (WIN32)
    if(MSVC)
        set(LIBCLANG_LDFLAGS -Wl,${LLVM_DIR}/lib/libclang.lib)
    else()
        set(LIBCLANG_LDFLAGS -L${LLVM_DIR}/lib -Wl,-rpath,${LLVM_DIR}/lib -llibclang.dll)
    endif()
else (WIN32)
    set(LIBCLANG_LDFLAGS -L${LLVM_LIBDIR} -Wl,-rpath,${LLVM_LIBDIR} -lclang)
endif (WIN32)

if(BUILD_BINDGEN)
    add_custom_command(OUTPUT ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX}
        DEPENDS
            ceramic
            ${BINDGEN_SOURCES}
        COMMAND ceramic
            -I${ceramic_SOURCE_DIR}/lib-ceramic
            -I${ceramic_SOURCE_DIR}/tools
            -o ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX}
            ${LIBCLANG_LDFLAGS}
            ${ceramic_SOURCE_DIR}/tools/bindgen.ceramic)

    add_custom_target(ceramic-bindgen-target ALL
        DEPENDS ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX})
    install(PROGRAMS ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION bin)
endif(BUILD_BINDGEN)
