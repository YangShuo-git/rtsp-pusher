add_subdirectory(encoder)
add_subdirectory(pusher)
add_subdirectory(capturer)

aux_source_directory(. SRCS)
add_library(pushWork SHARED ${SRCS})

target_link_libraries(pushWork encoder pusher capturer tools)
