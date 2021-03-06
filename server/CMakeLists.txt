add_definitions(-DSERVER -std=gnu++0x -DBINARY_ARCH=${OF_TARGET_ARCH} -DBINARY_OS=${OF_PLATFORM_BIN} -D_DEBUG)

if(NOT DEFINED WIN32 AND NOT DEFINED APPLE)
    set(EXTRA_LIBS ${EXTRA_LIBS} -pthread)
elseif(APPLE)
    set(EXTRA_LIBS ${EXTRA_LIBS})
elseif(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} ws2_32 winmm)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(EXTRA_LIBS ${EXTRA_LIBS} dl rt)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    set(EXTRA_LIBS ${EXTRA_LIBS} socket nsl)
endif()

set(SERVER_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/crypto.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/stream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../shared/tools.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/command.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/worldio.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../engine/lua.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/entities.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../fpsgame/messageSystem.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core_bindings.cpp)

IF(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
message("-- Compiling server in debug mode")
add_definitions( -O3 -DLOG_ENABLE_TRACE -DLOG_ENABLE_DEBUG)
ENDIF(CMAKE_BUILD_TYPE STREQUAL "DEBUG")

add_executable(${OF_SERVER_BINARY} ${SERVER_SOURCES})
target_link_libraries(${OF_SERVER_BINARY}
	${ENET_LIBRARIES}
	${LUVIT_LIBRARIES}
	${ZLIB_LIBRARIES}
	${EXTRA_LIBS})

install(TARGETS
	${OF_SERVER_BINARY}
	RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
	LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}
	ARCHIVE DESTINATION ${INSTALL_ARCHIVE_DIR})
