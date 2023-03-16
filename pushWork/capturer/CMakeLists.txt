
aux_source_directory(. SRCS)
add_library(capturer SHARED ${SRCS})

# 为什么同级的目录，只是需要在一个目录写上共用的链接库
# target_link_libraries(capturer avfilter avformat avcodec avutil swresample swscale)

