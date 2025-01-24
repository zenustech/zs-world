cmake_minimum_required(VERSION 3.20)

set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

include("project_info.in")

include(CheckLanguage)
check_language(CUDA)
if (CMAKE_CUDA_COMPILER)
	enable_language(CUDA)
endif()

project(zs-world
	VERSION ${project_version}
	DESCRIPTION ${project_description}
	LANGUAGES C CXX)

option(ZS_WORLD_ENABLE_DOC "Build Doc" OFF)
option(ZS_ENABLE_USD "Build USD module" ON)

if (CMAKE_VERSION VERSION_LESS "3.21")
    # ref: VulkanMemoryAllocator
    # https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html
    string(COMPARE EQUAL ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} PROJECT_IS_TOP_LEVEL)
endif()
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_path(GET CMAKE_CXX_COMPILER STEM LAST_ONLY ZPC_CXX_COMPILER_BINARY)
message(STATUS "Current C++ compiler is ${ZPC_CXX_COMPILER_BINARY} (${CMAKE_CXX_COMPILER})")
if(${ZPC_CXX_COMPILER_BINARY} STREQUAL "icx")
    add_compile_options(-Wno-error=register -Wno-enum-constexpr-conversion -Wno-implicit-function-declaration)
endif()

# 
set(ZS_ENABLE_VULKAN ON)
# set(ZS_ENABLE_VULKAN_VALIDATION OFF)

add_subdirectory(deps/interface EXCLUDE_FROM_ALL)

if (ZS_WORLD_ENABLE_DOC)
    add_subdirectory(doc)
endif()

###########
## deps  ##
###########

add_subdirectory(deps/utf8)

add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE deps/tinygltf)

add_definitions(-DBOOST_SYSTEM_USE_UTF8)
if (WIN32)
	add_definitions(-D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN)
endif(WIN32)
find_package(Boost REQUIRED COMPONENTS system filesystem)

###########
## world ##
###########
# 'zs_world_boost' mainly for the plugin system
add_library(zs_world_boost STATIC)
target_sources(zs_world_boost PRIVATE
	# nodes
	zs/world/node/NodeManager.cpp
)
target_link_libraries(zs_world_boost PRIVATE zs_interface Boost::system Boost::filesystem zensim)
target_include_directories(zs_world_boost PUBLIC ${Boost_INCLUDE_DIR})
target_compile_definitions(zs_world_boost PRIVATE -DZs_World_EXPORT)
# https://stackoverflow.com/questions/75121130/mac-os-boost-installed-using-macports-collision-with-c-version
target_compile_definitions(zs_world_boost PUBLIC -DBOOST_ASIO_HAS_STD_INVOKE_RESULT)
set_target_properties(zs_world_boost 
  	PROPERTIES 
	LINKER_LANGUAGE CXX
	POSITION_INDEPENDENT_CODE ON
)

# slang for render
option(ZS_SLANG_BUILD_FROM_SOURCE "OFF will use prebuilt binary from github. ON will add slang as subdirectory" ON)

set(SLANG_SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/slang)
set(SLANG_ENABLE_EXTERNAL_COMPILER_WARNINGS OFF BOOL CACHE INTERNAL "Disable slang warnings")
set(SLANG_SLANG_LLVM_FLAVOR DISABLE)
set(SLANG_ENABLE_GFX OFF)
set(SLANG_ENABLE_SLANGRT OFF)
if (ZS_SLANG_BUILD_FROM_SOURCE)
	add_subdirectory(${SLANG_SOURCE_DIR} EXCLUDE_FROM_ALL)
else ()
	include(ExternalProject)
	set(SLANG_SOURCE_URL "https://github.com/shader-slang/slang/archive/refs/tags/v2024.14.tar.gz")
	if(WIN32)
		set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v2024.14/slang-2024.14-windows-x86_64.tar.gz")
	elseif(APPLE)
		set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v2024.14/slang-2024.14-macos-x86_64.tar.gz")
	elseif(UNIX)
		set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v2024.14/slang-2024.14-linux-x86_64.tar.gz")
	else()
		message(FATAL_ERROR "Unsupported platform. Only Windows, MacOS, and Linux are supported. Please turn ON ZS_SLANG_BUILD_FROM_SOURCE flag.")
	endif()

	# slang prebuilt dir
	set(SLANG_PREBUILT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externals/slang/prebuilt" CACHE INTERNAL "Prebuilt slang binary path")

	file(MAKE_DIRECTORY "${SLANG_PREBUILT_DIR}")
	if (NOT EXISTS "${SLANG_PREBUILT_DIR}/bin")
		set(SLANG_TARBALL "${SLANG_PREBUILT_DIR}/slang.tar.gz")
		message(STATUS "Downloading Slang from ${SLANG_URL}")
		file(DOWNLOAD ${SLANG_URL} ${SLANG_TARBALL} SHOW_PROGRESS)
		execute_process(
				COMMAND tar -xzf "${SLANG_TARBALL}" -C "${SLANG_PREBUILT_DIR}"
				RESULT_VARIABLE TAR_RESULT)
	endif ()
	set(SLANG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externals/slang/source")
	file(MAKE_DIRECTORY "${SLANG_SOURCE_DIR}")
	if (NOT EXISTS "${SLANG_SOURCE_DIR}/slang-2024.14")
		set(SLANG_SOURCE_TARBALL "${SLANG_SOURCE_DIR}/slang-src.tar.gz")
		message(STATUS "Downloading Slang source from ${SLANG_SOURCE_URL}")
		file(DOWNLOAD ${SLANG_SOURCE_URL} ${SLANG_SOURCE_TARBALL} SHOW_PROGRESS)
		execute_process(
				COMMAND tar -xzf "${SLANG_SOURCE_TARBALL}" -C "${SLANG_SOURCE_DIR}"
				RESULT_VARIABLE TAR_RESULT)
	endif ()
	set(SLANG_SOURCE_DIR "${SLANG_SOURCE_DIR}/slang-2024.14")
	link_directories(BEFORE "${SLANG_PREBUILT_DIR}/bin" "${SLANG_PREBUILT_DIR}/lib")

	set(SLANG_PREBUILT_PATH "${SLANG_PREBUILT_DIR}" CACHE PATH "Path to the Slang prebuilt binaries.")
	set(SLANG_PREBUILT_INCLUDE_DIR "${SLANG_PREBUILT_DIR}/include" CACHE PATH "Path to the Slang prebuilt headers.")
	execute_process(
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${SLANG_PREBUILT_DIR}/bin" "${CMAKE_BINARY_DIR}"
	)
endif ()

add_subdirectory(zs/world/render)

# zs_world
add_library(zs_world SHARED)
set_target_properties(zs_world
	PROPERTIES
	POSITION_INDEPENDENT_CODE ON

	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}

	SKIP_BUILD_RPATH OFF
	BUILD_WITH_INSTALL_RPATH TRUE
	INSTALL_RPATH "\$ORIGIN"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)
target_include_directories(zs_world PUBLIC zs)
target_sources(zs_world PRIVATE
	zs/world/World.cpp
	zs/world/SceneInterface.cpp

	# core
	zs/world/core/Command.cpp
	zs/world/core/Signal.cpp
	zs/world/core/Utils.cpp
	zs/world/core/Archive.cpp

	# geometry
	zs/world/geometry/SimpleGeom.cpp

	# scene
	zs/world/scene/SceneContext.cpp
	zs/world/scene/Camera.cpp

	zs/world/scene/Primitive.cpp
	zs/world/scene/PrimitiveQuery.cpp
	zs/world/scene/PrimitiveToOtherForms.cpp
	zs/world/scene/PrimitiveFromOtherForms.cpp
	zs/world/scene/PrimitiveTransform.cpp

	zs/world/scene/PrimitiveOperation.cpp

	zs/world/scene/PrimitiveRenderer.cpp
	zs/world/scene/PrimitiveSerializer.cpp
	
	# nodes
	zs/world/node/Context.cpp
	zs/world/node/Nodes.cpp

	# async
	zs/world/async/Event.cpp
	zs/world/async/Executor.cpp
	# systems
	zs/world/system/ZsExecSystem.cpp
	zs/world/system/ResourceSystemPrimitive.cpp
	zs/world/system/ResourceSystemLog.cpp
	zs/world/system/ResourceSystem.cpp
	zs/world/system/PyExecSystem.cpp
)
# target_link_libraries(zpc_world PRIVATE $<$<CXX_COMPILER_ID:MSVC>:Ws2_32>)
# https://stackoverflow.com/questions/78170391/why-is-jthread-not-presented-in-libc-using-clang-18-c26
if (${CMAKE_CXX_COMPILER_ID} MATCHES "AppleClang")
  target_compile_options(zs_world PUBLIC
      $<$<COMPILE_LANGUAGE:CXX>: -std=c++20 -fexperimental-library>
  )
else()
  target_compile_features(zs_world PUBLIC cxx_std_20)
endif()
target_link_libraries(zs_world PUBLIC zs_interface zs_world_boost zensim zpc::shader_runtime 
	tinygltf utf8cpp)
target_link_libraries(zs_world PRIVATE zpc_jit_py)
if (WIN32)
	target_link_libraries(zs_world PRIVATE synchronization)	# for Event.cpp
endif()
target_compile_definitions(zs_world PRIVATE -DZs_World_EXPORT)

########################
## additional modules ##
########################

# plugin
function(zs_add_plugin name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG
    ";" "" ""
    )

  add_library(${name} SHARED ${ARG_UNPARSED_ARGUMENTS})

  set_target_properties(${name} 
  	PROPERTIES 
	LINKER_LANGUAGE CXX
	POSITION_INDEPENDENT_CODE ON
	#
	# ref:https://stackoverflow.com/questions/32028667/are-cmake-source-dir-and-project-source-dir-the-same-in-cmake
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
	#
	# ref: https://stackoverflow.com/questions/58360502/how-to-set-rpath-origin-in-cmake
	# ref: https://stackoverflow.com/questions/57915564/cmake-how-to-set-rpath-to-origin-with-cmake
	SKIP_BUILD_RPATH OFF
	BUILD_WITH_INSTALL_RPATH TRUE
	INSTALL_RPATH "\$ORIGIN"
	INSTALL_RPATH_USE_LINK_PATH TRUE
  )
  target_link_libraries(${name} PRIVATE
	zensim
	zs_interface
  )

  add_dependencies(zs_world ${name})
endfunction()

#
zs_add_plugin(examplePlugin zs/plugins/ExamplePlugin.cpp)
target_link_libraries(examplePlugin PUBLIC zswhereami)
zs_add_plugin(exceptionPlugin zs/plugins/ExceptionNode.cpp)
zs_add_plugin(longRuntimePlugin zs/plugins/LongRuntimeNode.cpp)
zs_add_plugin(longOutputPlugin zs/plugins/LongOutputNode.cpp)

# usd
if(ZS_ENABLE_USD)

	if (NOT DEFINED ZS_USD_ROOT OR NOT EXISTS ${ZS_USD_ROOT})
		set(ZS_USD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/externals/USD")
		if (NOT EXISTS ${ZS_USD_ROOT})
			set(ZS_ENABLE_USD OFF)
			message(STATUS "When ZPC_USE_ENABLED, ZS_USD_ROOT must be set, or the USD package should be placed at externals/USD.")
		endif()
	endif()
endif()

if(ZS_ENABLE_USD)
	list(APPEND CMAKE_PREFIX_PATH ${ZS_USD_ROOT})
	# add_subdirectory(projects/USD)
	target_compile_definitions(zs_world PUBLIC -DZS_ENABLE_USD=1)
	target_sources(zs_world PRIVATE
		zs/world/usd/USDSceneManager.cpp
		# zs/world/usd/USDNodeAligner.cpp
		zs/world/usd/USDSceneDesc.cpp
		zs/world/usd/USDScenePrim.cpp
		zs/world/usd/USDSceneLight.cpp
	)
	if (WIN32)
		set(pxr_DIR ${ZS_USD_ROOT})
	#elseif(MACOS) # TODO
	#elseif(LINUX) # TODO
	endif()

	message("pxr_DIR is : ${ZS_USD_ROOT}")

	find_package(OpenGL REQUIRED)
	set(pxr_DIR "${ZS_USD_ROOT}")
	find_package(pxr REQUIRED)
	target_include_directories(zs_world PRIVATE ${PXR_INCLUDE_DIRS})
	target_link_directories(zs_world PRIVATE ${ZS_USD_ROOT}/lib)
	find_package(TBB REQUIRED)
	target_link_libraries(zs_world PRIVATE
		zpc_jit_py TBB::tbb
		${PXR_LIBRARIES}
	)

	message(STATUS "iterating pxr libs: ${PXR_LIBRARIES}")
	foreach(usdlib IN LISTS PXR_LIBRARIES)
		add_custom_command(
			TARGET zs_world
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${usdlib}> $<TARGET_FILE_DIR:zs_world>/$<TARGET_FILE_BASE_NAME:${usdlib}>$<TARGET_FILE_SUFFIX:${usdlib}>
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${ZS_USD_ROOT}/lib/usd $<TARGET_FILE_DIR:zs_world>/usd
			VERBATIM
			COMMENT "copying linked USD shared library [$<TARGET_FILE:${usdlib}>]"
		)
		get_target_property(usd_dep_libs ${usdlib} INTERFACE_LINK_LIBRARIES)
		message(STATUS "\titerating pxr-dependent libs of (${usdlib}): ${usd_dep_libs}")
		foreach(lib ${usd_dep_libs})
			if (TARGET ${lib})
				message(STATUS "\t\titerating target-type dependent lib: ${lib}")
				get_target_property(lib_type ${lib} TYPE)
				if (lib_type STREQUAL "SHARED_LIBRARY")
					add_custom_command(
						TARGET zs_world
						POST_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${lib}> $<TARGET_FILE_DIR:zs_world>/$<TARGET_FILE_BASE_NAME:${lib}>$<TARGET_FILE_SUFFIX:${lib}>
						VERBATIM
						COMMENT "copying USD-dependent runtime shared libraries (target) [$<TARGET_FILE:${lib}>]"
    					)
				endif(lib_type STREQUAL "SHARED_LIBRARY")
			elseif(EXISTS ${lib})	# file
				message(STATUS "\t\titerating file-type dependent lib: ${lib}")
				cmake_path(GET lib EXTENSION suffix)
				cmake_path(GET lib STEM basename)
				set(FILE_IS_SHARED 0)
				if (WIN32)
				#[=[
					add_custom_command(
						TARGET zs_world
						POST_BUILD
						COMMAND ${CMAKE_COMMAND} 
							-DOBJECTS=$<TARGET_RUNTIME_DLLS:zs_world> 
							-DOUTPUT=$<TARGET_FILE_DIR:zs_world>
							-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_files.cmake
						VERBATIM
						COMMENT "copying USD-dependent runtime shared libraries (lib-corresponding dll file) [$<TARGET_RUNTIME_DLLS:zs_world>]"
    				)
				#]=]
				elseif(APPLE)
					if (suffix MATCHES ".*\.dylib$")
						set(FILE_IS_SHARED 1)
					endif()
				else(UNIX)	# 
					if(suffix MATCHES "^\.so.*$")
						set(FILE_IS_SHARED 1)
					endif()
				endif()
				if (FILE_IS_SHARED)
					add_custom_command(
						TARGET zs_world
						POST_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy ${lib} $<TARGET_FILE_DIR:zs_world>/${basename}${suffix}
						VERBATIM
						COMMENT "copying USD-dependent runtime shared libraries (file) [${lib}]"
    				)
				endif()
			endif(TARGET ${lib})
		endforeach(lib)
		message(STATUS "iterating pxr lib: ${usdlib}")
	endforeach()
endif()

# jit
if (ZS_ENABLE_JIT)

	target_sources(zs_world PRIVATE
		zs/world/py_zpc_jit/PyZpc.cpp
	)

	#################################
	## additional zpc_world module ##
	#################################
	set(PREFIX "lib")
	if (WIN32)
		set(SUFFIX ".dll")
		set(PREFIX "")
	elseif (APPLE)
		set(SUFFIX ".dylib")
	elseif (UNIX)
		set(SUFFIX ".so")
	else()
		message(FATAL_ERROR "unknown system to determine shared library suffix")
	endif()
	target_compile_definitions(zs_world PRIVATE -DZPC_OUTPUT_BINARY_CAPIS="${PREFIX}zpc_py_interop${SUFFIX}")
	target_compile_definitions(zs_world PRIVATE -DZPC_OUTPUT_BINARY_CLANG="${PREFIX}zpc_jit_clang${SUFFIX}")
	if (ZS_ENABLE_CUDA)
		target_compile_definitions(zs_world PRIVATE -DZPC_OUTPUT_BINARY_NVRTC="${PREFIX}zpc_jit_nvrtc${SUFFIX}")
	endif(ZS_ENABLE_CUDA)


	add_custom_target(zpcjit_postprocess)
	add_dependencies(zs_world zpcjit_postprocess)
	add_dependencies(zpcjit_postprocess zpc_jit_py)
	# ZPC_BIN_DIR
	get_target_property(ZPC_BIN_DIR zpc_py_interop LIBRARY_OUTPUT_DIRECTORY)
	set(ENV{ZPC_BIN_DIR} "${ZPC_BIN_DIR}")
	#
	# zpcjit directory as python package
	#
	add_custom_command(
		TARGET zpcjit_postprocess
		POST_BUILD
		COMMAND 
		${ZS_OVERWRITE_PYTHON_EXECUTABLE} -m ensurepip
		COMMENT "preparing pip module"
	)
	add_custom_command(
		TARGET zpcjit_postprocess
		POST_BUILD
		COMMAND 
		${ZS_OVERWRITE_PYTHON_EXECUTABLE} -m pip install numpy
		COMMENT "installing numpy"
	)

	#
	# zpcjit directory as python package
	#
	set(PY_LIBS_DIR ${ZPC_BIN_DIR}/resource/py_libs)
	add_custom_command(
		TARGET zpcjit_postprocess
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PY_LIBS_DIR}/zpcjit
		# COMMAND ${CMAKE_COMMAND} -E make_directory ${ZPC_BIN_DIR}/zpcjit/zpc_jit/lib
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PY_LIBS_DIR}/zpcjit/zpc_jit/zpc/include/zensim
		COMMENT "creating pyzpc jit module directory at ${PY_LIBS_DIR}/zpcjit"
	)
	file(GLOB ZPC_JIT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/deps/zpc_jit/pyzpc/*.py)
	# message("ZPC_JIT_SRC : ${ZPC_JIT_SRC}")

	foreach(ZPCJIT_FILE IN LISTS ZPC_JIT_SRC)
		add_custom_command(
			TARGET zpcjit_postprocess POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZPCJIT_FILE} ${PY_LIBS_DIR}/zpcjit
			COMMENT "Copying zpcjit src file: ${ZPCJIT_FILE}")
	endforeach()

	# prepare zpc headers for pyzpc
	add_custom_command(
		TARGET zpcjit_postprocess POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory 
			${CMAKE_CURRENT_SOURCE_DIR}/deps/interface/zpc/include/zensim 
			${PY_LIBS_DIR}/zpcjit/zpc_jit/zpc/include/zensim
		COMMENT "Copying zpc source files: ${CMAKE_CURRENT_SOURCE_DIR}/zpc/include/zensim")

	#
	# copy python environment
	#
	if (WIN32)
		# windows
		cmake_path(GET ZS_OVERWRITE_PYTHON_EXECUTABLE PARENT_PATH PYTHON_ENV_PATH)
		message(STATUS "python3 parent: ${PYTHON_ENV_PATH}")
		if (NOT EXISTS ${ZPC_BIN_DIR}/Lib/site-packages)
			add_custom_command(
        			TARGET zpcjit_postprocess
        			POST_BUILD
        			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/Lib ${ZPC_BIN_DIR}/lib
        			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/DLLs ${ZPC_BIN_DIR}/DLLs

        			COMMENT "copying python lib directories at ${PYTHON_ENV_PATH} to ${ZPC_BIN_DIR}"
      			)
    		endif()
		if (EXISTS ${PYTHON_ENV_PATH}/library)
			add_custom_command(
				TARGET zpcjit_postprocess
				POST_BUILD
        			COMMAND ${CMAKE_COMMAND} -E copy_directory "${PYTHON_ENV_PATH}/library" ${ZPC_BIN_DIR}/library
        			COMMENT "copying python library directory at ${PYTHON_ENV_PATH} to ${ZPC_BIN_DIR}"
      			)
    		endif()
    		if (EXISTS ${PYTHON_ENV_PATH}/zlib.dll)
			add_custom_command(
				TARGET zpcjit_postprocess
				POST_BUILD
        			COMMAND ${CMAKE_COMMAND} -E copy "${PYTHON_ENV_PATH}/zlib.dll" ${ZPC_BIN_DIR}
				COMMENT "copying python zlib.dll ${PYTHON_ENV_PATH} to ${ZPC_BIN_DIR}"
			)
		endif()
		foreach(lib ${ZS_PYTHON_LIBS})
			add_custom_command(
				TARGET zpcjit_postprocess
				POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy lib $<TARGET_FILE_DIR:zpc_py_interop>
			)
		endforeach()
	elseif(APPLE)
		# apple
		cmake_path(GET ZS_OVERWRITE_PYTHON_EXECUTABLE PARENT_PATH PYTHON_ENV_PATH)
		cmake_path(GET PYTHON_ENV_PATH PARENT_PATH PYTHON_ENV_PATH)
		message(STATUS "python3 parent: ${PYTHON_ENV_PATH}")
		if (NOT EXISTS ${ZPC_BIN_DIR}/lib/pkgconfig)
			add_custom_command(
				TARGET zpcjit_postprocess
				POST_BUILD
        			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/bin ${ZPC_BIN_DIR}/bin
        			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/lib ${ZPC_BIN_DIR}/lib

        			COMMENT "copying python lib directory ${PYTHON_ENV_PATH}/lib to ${ZPC_BIN_DIR}/lib"
      			)
		endif()
  	else(WIN32)
		# linux
		cmake_path(GET ZS_OVERWRITE_PYTHON_EXECUTABLE PARENT_PATH PYTHON_ENV_PATH)
		cmake_path(GET PYTHON_ENV_PATH PARENT_PATH PYTHON_ENV_PATH)
		message(STATUS "python3 parent: ${PYTHON_ENV_PATH}")
		if (NOT EXISTS ${ZPC_BIN_DIR}/lib/libtinfo.a)
			add_custom_command(
				TARGET zpcjit_postprocess
				POST_BUILD
        			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/bin ${ZPC_BIN_DIR}/bin
        			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYTHON_ENV_PATH}/lib ${ZPC_BIN_DIR}/lib

        			COMMENT "copying python lib directory ${PYTHON_ENV_PATH}/lib to ${ZPC_BIN_DIR}/lib"
      			)
		endif()
	endif(WIN32)
endif(ZS_ENABLE_JIT)