add_definitions(-DCLIENT -std=gnu++0x -DHAVE_LIBUV -DBINARY_ARCH=${OF_TARGET_ARCH} -DBINARY_OS=${OF_PLATFORM_BIN})

if(NOT DEFINED WIN32 AND NOT DEFINED APPLE)
    set(EXTRA_LIBS ${EXTRA_LIBS} SDL2 SDL2_image SDL2_mixer -pthread)
elseif(APPLE)
    set(EXTRA_LIBS ${EXTRA_LIBS} ${SDL2_LIBRARY} ${SDL2_IMAGE_LIBRARY}
        ${SDL2_MIXER_LIBRARY})
elseif(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} SDL2 SDL_image SDL2_mixer opengl32
        ws2_32 winmm)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(EXTRA_LIBS ${EXTRA_LIBS} dl rt)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    set(EXTRA_LIBS ${EXTRA_LIBS} socket nsl)
endif()

set(CLIENT_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/crypto
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/geom
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/glemu
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/stream
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/tools
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/zip
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/3dgui
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/aa
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/bih
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/blend
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/client
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/command
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/console
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/decal
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/dynlight
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/grass
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/light
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/main
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/material
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/menus
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/movie
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/normal
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/octa
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/octaedit
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/octarender
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/physics
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/pvs
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendergl
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/renderlights
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendermodel
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/renderparticles
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendersky
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendertext
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/renderva
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/server
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/serverbrowser
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/shader
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/sound
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/texture
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/water
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/world
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/worldio
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/lua
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/ai
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/client
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/entities
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/fps
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/render
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/scoreboard
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/server
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/waypoint
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/weapon
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/messageSystem)

IF(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
	message("-- Compiling client in debug mode")
	add_definitions( -O3 -DLOG_ENABLE_TRACE -DLOG_ENABLE_DEBUG)
ENDIF()

add_executable(${OF_CLIENT_BINARY} ${CLIENT_SOURCES})
target_link_libraries(${OF_CLIENT_BINARY}
	${ENET_LIBRARIES}
	${OPENGL_LIBRARIES}
	${ZLIB_LIBRARIES}
	${EXTRA_LIBS}
	${LUVIT_LIBRARIES}
)

install(TARGETS
	${OF_CLIENT_BINARY}
	RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
	LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}
	ARCHIVE DESTINATION ${INSTALL_ARCHIVE_DIR})
