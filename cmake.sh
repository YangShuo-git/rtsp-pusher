if [ -d build ]; then 
    rm -rf build
fi

if [ -e struct_file ]; then
    rm struct_file
fi

mkdir build

cd build

echo "------start to build------"
cmake ..
make
echo "------end to build------"
