if [ -d build ]; then 
    rm -rf build
fi

if [ -e *.pcm ]; then
    rm *.pcm
echo "------delete pcm file------"
fi

if [ -e *.aac ]; then
    rm *.aac
echo "------delete aac file------"
fi

if [ -e *.yuv ]; then
    rm *.yuv
echo "------delete pcm file------"
fi

if [ -e *.h264 ]; then
    rm *.h264
echo "------delete aac file------"
fi

mkdir build

cd build

echo "------start to build------"
cmake ..
make
echo "------end to build------"
