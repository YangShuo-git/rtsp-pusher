
aux_source_directory(. SRCS)
add_library(log SHARED ${SRCS})

#如果不加上下行代码，则会报出“找不到头文件demo_thread”的错误
# target_include_directories(log PUBLIC /home/yangshuo/cmake_demo/demo_thread)

