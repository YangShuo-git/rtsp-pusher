if [ -d build ]; then 
    rm -rf build
fi

mkdir build

cd build

echo "------start to build------"
cmake ..
make
echo "------end to build------"
