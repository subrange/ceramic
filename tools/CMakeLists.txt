option(BUILD_BINDGEN "Build the ceramic-bindgen tool for generating Ceramic bindings from C headers." OFF)

if(BUILD_BINDGEN)
    if (WIN32)
        if(MSVC)
            set(LIBCLANG_LDFLAGS -Wl,${LLVM_DIR}/lib/libclang.lib)
        else()
            set(LIBCLANG_LDFLAGS -L${LLVM_DIR}/lib -Wl,-rpath,${LLVM_DIR}/lib -llibclang.dll)
        endif()
    else()
        set(LIBCLANG_LDFLAGS -L${LLVM_LIBRARY_DIRS} -Wl,-rpath,${LLVM_LIBRARY_DIRS} -lclang)
    endif()

    add_custom_command(OUTPUT ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX}
        DEPENDS
            ceramic
            bindgen.crm
        COMMAND ceramic
            -I${ceramic_SOURCE_DIR}/lib-ceramic
            -I${ceramic_SOURCE_DIR}/tools
            -o ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX}
            ${LIBCLANG_LDFLAGS}
            ${ceramic_SOURCE_DIR}/tools/bindgen.crm)

    add_custom_target(ceramic-bindgen-target ALL
        DEPENDS ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX})
    install(PROGRAMS ${ceramic_BINARY_DIR}/tools/ceramic-bindgen${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION bin)
endif()
