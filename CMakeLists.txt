cmake_minimum_required(VERSION 2.8)

if(DEFINED TRAVIS_BUILD)
	set(INSTALL_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	set(INSTALL_ARCHIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	set(INSTALL_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(OF_TARGET_ARCH "x64")
	else()
		set(OF_TARGET_ARCH "x86")
	endif()
	
	set(OF_CXX_FLAGS "-fomit-frame-pointer -Wall -fsigned-char -fno-exceptions -fno-rtti")
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(OF_PLATFORM_SRC "linux")
        set(OF_PLATFORM_BIN "linux")
    elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        set(OF_PLATFORM_SRC "freebsd")
        set(OF_PLATFORM_BIN "freebsd")
    elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
        set(OF_PLATFORM_SRC "solaris")
        set(OF_PLATFORM_BIN "solaris")
    else()
        message(FATAL_ERROR "Unsupported operating system.")
    endif()
    
    message("WARNING:")
    message("Travis build, please do not use on non travis!")
	message("")
endif()

IF(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG")
message("Building in debug mode")
ENDIF(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG")

find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)

find_package(PkgConfig)

message(STATUS "Detecting LuaJIT...")
if(NOT DEFINED WIN32 AND NOT OF_LUAJIT_LOCAL AND PKG_CONFIG_FOUND)
    message(STATUS "Using pkg-config to detect LuaJIT...")
    pkg_check_modules(LUAJIT luajit)
    if(LUAJIT_FOUND)
        message(STATUS "Found LuaJIT.")
		message(STATUS "include: ${LUAJIT_INCLUDE_DIRS}")
        include_directories(${LUAJIT_INCLUDE_DIRS})
        link_directories(${LUAJIT_LIBRARY_DIRS})
        set(EXTRA_LIBS ${LUAJIT_LIBRARIES})
    else()
        message(FATAL_ERROR "LuaJIT not found.")
    endif()
else()
    message(STATUS, "Using local LuaJIT.")
    set(EXTRA_LIBS luajit)
endif()

if(APPLE)
    set(EXTRA_LIBS ${EXTRA_LIBS} -pagezero_size 10000 -image_base 100000000)
endif()

message(STATUS "Detecting SDL2...")
if(NOT DEFINED WIN32 AND NOT DEFINED APPLE)
    include_directories(/usr/X11R6/include)
    if(NOT OF_SDL2_LOCAL AND PKG_CONFIG_FOUND)
        message(STATUS "Using pkg-config to detect SDL2...")
        pkg_check_modules(SDL2 sdl2)
        if(SDL2_FOUND)
            message(STATUS "Found SDL2.")
            message(STATUS "include: ${SDL2_INCLUDE_DIRS}")
            include_directories(${SDL2_INCLUDE_DIRS})
            link_directories(${SDL2_LIBRARY_DIRS})
        else()
            message(FATAL_ERROR "SDL2 not found.")
        endif()
    else()
        message(STATUS "Using local SDL2.")
    endif()
    add_definitions(-D_REENTRANT -D_THREAD_SAFE)
elseif(APPLE)
    find_library(SDL2_LIBRARY SDL2)
    find_library(SDL2_IMAGE_LIBRARY SDL2_image)
    find_library(SDL2_MIXER_LIBRARY SDL2_mixer)

    find_path(SDL2_INCLUDE_DIR SDL.h)
    find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h)
    find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h)
    include_directories(${SDL2_INCLUDE_DIR} ${SDL2_IMAGE_INCLUDE_DIR}
        ${SDL2_MIXER_INCLUDE_DIR})
endif()

add_subdirectory(libraries/enet)
add_subdirectory(libraries/uv)
add_subdirectory(libraries/luvit)

include_directories(
	${ENET_INCLUDE_DIRS}
	${UV_INCLUDE_DIRS}
	${LUVIT_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/shared
	${CMAKE_CURRENT_SOURCE_DIR}/engine
	${CMAKE_CURRENT_SOURCE_DIR}/fpsgame
)

set(OF_CLIENT_BINARY client_${OF_PLATFORM_BIN}_${OF_TARGET_ARCH})
set(OF_SERVER_BINARY server_${OF_PLATFORM_BIN}_${OF_TARGET_ARCH})

#client
add_subdirectory(client)
add_subdirectory(server)


#end client

# todo: server



