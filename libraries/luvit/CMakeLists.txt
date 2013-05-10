project (luvit C)

message(${UV_INCLUDE_DIRS})

set(LUVIT_INCLUDE_DIRS
	${UV_INCLUDE_DIRS}
	${LUAJIT_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/src
	CACHE INTERNAL "Luvit include directories"
)

set(LUVIT_LIBRARY luvit)

include_directories(${LUVIT_INCLUDE_DIRS})
add_library(${LUVIT_LIBRARY}
	${CMAKE_CURRENT_SOURCE_DIR}/src/lconstants.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/lenv.c
#	${CMAKE_CURRENT_SOURCE_DIR}/src/lhttp_parser.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/los.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_debug.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_dns.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_fs.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_fs_watcher.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_handle.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luvit_exports.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luvit_init.c
#	${CMAKE_CURRENT_SOURCE_DIR}/src/luvit_main.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_misc.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_pipe.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_process.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_stream.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_tcp.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_timer.c
#	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_tls.c
#	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_tls_conn.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_tty.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_udp.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/luv_zlib.c
#	${CMAKE_CURRENT_SOURCE_DIR}/src/lyajl.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils.c)

set(LUVIT_LIBRARIES ${LUVIT_LIBRARY} ${UV_LIBRARIES} CACHE INTERNAL "Luvit libraries")
	
