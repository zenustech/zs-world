set(PRIVATE_SOURCE
        private/shader_runtime.cpp
        private/virtual_file_helper.cpp
        private/impl/virtual_filesystem.cpp
        private/impl/shader_function.cpp
)

add_library(render_shader_runtime SHARED ${PRIVATE_SOURCE})
add_library(zpc::shader_runtime ALIAS render_shader_runtime)

target_include_directories(render_shader_runtime PUBLIC public PRIVATE private)

target_link_libraries(render_shader_runtime
        PUBLIC
        zpc
)

if (ZS_SLANG_BUILD_FROM_SOURCE)
    target_link_libraries(render_shader_runtime
        PRIVATE
        slang core compiler-core
    )
else ()
    file(GLOB SLANG_STATIC_LIBRARIES_RELATIVE "${SLANG_PREBUILT_DIR}/lib/*")
    set(SLANG_STATIC_LIBRARIES)
    foreach(lib IN LISTS SLANG_STATIC_LIBRARIES_RELATIVE)
        get_filename_component(lib_abs "${lib}" ABSOLUTE)
        list(APPEND SLANG_STATIC_LIBRARIES "${lib_abs}")
    endforeach()
    target_link_libraries(render_shader_runtime
        PRIVATE
        ${SLANG_STATIC_LIBRARIES}
    )
    target_include_directories(render_shader_runtime
            PRIVATE "${SLANG_PREBUILT_INCLUDE_DIR}" "${SLANG_SOURCE_DIR}/source")
endif ()

set_target_properties(render_shader_runtime PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

include(GenerateExportHeader)
generate_export_header(render_shader_runtime
        EXPORT_MACRO_NAME "RENDER_SHADER_RUNTIME_API"
        NO_EXPORT_MACRO_NAME "RENDER_SHADER_RUNTIME_INTERNAL"
        EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/ExportHeaders/render_shader_runtime_export.h"
)
target_include_directories(render_shader_runtime PUBLIC ${CMAKE_BINARY_DIR}/ExportHeaders)

# This is required to prevent program dynamic linking to slang bring with VulkanSDK
if (ZS_SLANG_BUILD_FROM_SOURCE)
    add_custom_command(TARGET render_shader_runtime POST_BUILD
            # slang
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:slang>
            $<TARGET_FILE_DIR:render_shader_runtime>
    )
    if (WIN32)
        # add_custom_command(TARGET render_shader_runtime POST_BUILD
        #         # slang
        #         COMMAND ${CMAKE_COMMAND} -E copy_if_different
        #         $<TARGET_PDB_FILE:slang>
        #         $<TARGET_FILE_DIR:render_shader_runtime>
        # )

        # copy SLANG_CORE_LIBRARIES
        add_custom_command(TARGET render_shader_runtime POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:lz4_shared> $<TARGET_FILE_DIR:render_shader_runtime>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:miniz> $<TARGET_FILE_DIR:render_shader_runtime>
        )
    endif ()
endif ()

# Testing
add_executable(zpc_test_slang_virtual_filesystem EXCLUDE_FROM_ALL tests/test_slang_virtual_filesystem.cpp)
target_link_libraries(zpc_test_slang_virtual_filesystem zpc::shader_runtime)
add_test(NAME zpc_test_slang_virtual_filesystem COMMAND zpc_test_slang_virtual_filesystem)
set_target_properties(zpc_test_slang_virtual_filesystem PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
