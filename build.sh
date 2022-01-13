#!bash
build_mode=$1
function print_usage() {
    echo "++++++++++++++++++++++++++++++++++++++++++++++++++++"
    echo "sh build.sh all|make|clean"
    echo "- all: will update all env && make it"
    echo "- make: just do make, never update env"
    echo "- clean: make clean"
    echo "++++++++++++++++++++++++++++++++++++++++++++++++++++"
    exit 0
}
if [ $# -lt 1 ];then
    print_usage
fi

cd ~
user_dir=`pwd`
cd -

python_binary=${user_dir}/.jumbo/bin/python2.7
python_library=${user_dir}/.jumbo/lib/python2.7.so
python_include_dir=${user_dir}/.jumbo/include/python2.7
if [ ! -f ${python_binary} ];then
    echo "Miss python ${python_binary}, please install with this cmd: jumbo install python"
    exit -1
fi

#apply feed code
if [ -f "paddle/fluid/feed/apply_feed_code.sh" ];then
    sh paddle/fluid/feed/apply_feed_code.sh
fi 

function makeit() {
    cd build
    make -j8
    cd ..
}

function cmake_all() {
    mkdir build
    cd build
    #make clean
    cmake -DCMAKE_INSTALL_PREFIX=./output/ -DCMAKE_BUILD_TYPE=Release -DWITH_PYTHON=ON -DWITH_MKL=OFF -DWITH_GPU=OFF -DWITH_PSLIB=ON -DPYTHON_INCLUDE_DIR=${python_include_dir} -DPYTHON_LIBRARY=${python_library} -DPYTHON_EXECUTABLE=${python_binary} ..
    cd ..
}

if [ "${build_mode}" = "all" ];then
    cmake_all
    makeit
elif [ "${build_mode}" = "make" ];then
    makeit
elif "${build_mode}" = "clean" ];then
    cd build
    make clean
fi 
