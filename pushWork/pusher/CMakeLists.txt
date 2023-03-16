
aux_source_directory(. SRCS)
add_library(pusher SHARED ${SRCS})

# 为什么同级的目录，只是需要在一个目录写上共用的链接库
# 将三者最终链接到一起时，才会去链接三方库？
target_link_libraries(pusher avfilter avformat avcodec avutil swresample swscale)