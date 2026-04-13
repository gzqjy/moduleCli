#!/usr/bin/env bash

if [ $# -eq 0 ]; then
	echo "USAGE: $0 [amd64|arm64|mips64|loongarch64] buildNum"
	exit 1
fi

basepath=$(dirname $(readlink -f "$0"))
echo $basepath

## 收集 ELF 文件路径
#collect_elf_list() {
#    local search_dir="$1"
#    local elf_list_file="$2"
#
#    > "$elf_list_file"  # 清空输出文件
#
#    find "$search_dir" -type f | while read -r file; do
#        if file "$file" | grep -q "ELF"; then
#            echo "$file" >> "$elf_list_file"
#        fi
#    done
#}
#so_array=("cpucapi.so" "filecapi.so" "libboost_filesystem.so.1.87.0" "libboost_program_options.so.1.87.0" "libboost_thread.so.1.87.0" "libpcap.so.0.8" "libsqlite3.so.0" "libudev.so.1" "processcapi.so")
## 根据 ELF 列表收集依赖库
#collect_libs_from_elf_list() {
#    local elf_list_file="$1"
#    local output_dir="$2"
#
#    mkdir -p "$output_dir"
#
#    while read -r elf; do
#        echo "[*] Processing: $elf"
#        ldd "$elf" 2>/dev/null | awk '/=>/ {print $3}' | while read -r lib; do
#            if [[ -f "$lib" ]]; then
#                cp -vu "$lib" "$output_dir/"
#            fi
#        done
#    done < "$elf_list_file"
#}

# init
ARCH=$1
BUILD_NUM=$2
export BUILD_NUM=$BUILD_NUM
mkdir $basepath/build-$ARCH
mkdir $basepath/output-$ARCH

export BOOST_ROOT=/opt/boost_1_87_0

pushd $basepath/build-$ARCH
cmake .. -DARCH_NAME=$ARCH -DCMAKE_INSTALL_PREFIX=$basepath/output-$ARCH
if [ $? -ne 0 ]; then
    echo "构建失败，退出。"
    exit $?
fi

make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "构建失败，退出。"
    exit $?
fi
echo "构建成功！"
make -j$(( $(nproc) / 2 ))
make install
#cp $basepath/app.json $basepath/output-$ARCH/bin/app.json
mkdir $basepath/output-$ARCH/bin/lib -p

#elf_list="elf_files.txt"
#export LD_LIBRARY_PATH=$BOOST_ROOT/lib
#collect_elf_list "$basepath/output-$ARCH/bin" "$elf_list"
#collect_libs_from_elf_list "$elf_list" "$basepath/output-$ARCH/bin/lib"
# 收集 ELF 文件路径
#so_array=("libboost_filesystem.so.1.87.0" "libboost_program_options.so.1.87.0" "libboost_thread.so.1.87.0" "libpcap.so.0.8" "libsqlite3.so.0" "libudev.so.1")
if [ "$ARCH" == "amd64" ]; then
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_thread.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_program_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp $basepath/output-$ARCH/lib/libhv.so $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
#    cp $basepath/bin/hostEnv.json $basepath/output-$ARCH/bin/
#    cp $basepath/bin/diskStat.json $basepath/output-$ARCH/bin/
#    cp $basepath/bin/tools.iso $basepath/output-$ARCH/bin/
#    cp $basepath/bin/linux-amd64/VeraCrypt $basepath/output-$ARCH/bin/ -r
#    cp $basepath/3rd/secDisk/amd64/libUSecDiskShare.so $basepath/output-$ARCH/bin/lib/libUSecDisk.so.1.2.0 -r
#    cp /opt/boost_1_87_0/lib/libboost_thread.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/x86_64-linux-gnu/lib/libpcap.so.0.8 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/x86_64-linux-gnu/lib/libsqlite3.so.0 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/x86_64-linux-gnu/lib/libudev.so.1 $basepath/output-$ARCH/bin/lib
elif [ "$ARCH" == "arm64" ]; then
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_program_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_thread_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp $basepath/output-$ARCH/lib/libhv.so $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
#    cp $basepath/bin/hostEnv.json $basepath/output-$ARCH/bin/
#    cp $basepath/bin/diskStat.json $basepath/output-$ARCH/bin/
#    cp $basepath/bin/tools.iso $basepath/output-$ARCH/bin/
#    cp $basepath/bin/linux-arm64/VeraCrypt $basepath/output-$ARCH/bin/ -r
#    cp $basepath/3rd/secDisk/arm64/libUSecDiskShare.so $basepath/output-$ARCH/bin/lib/libUSecDisk.so.1.2.0 -r
#    cp /opt/boost_1_87_0/lib/libboost_thread.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/aarch64-linux-gnu/lib/libpcap.so.0.8 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/aarch64-linux-gnu/lib/libsqlite3.so.0 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/aarch64-linux-gnu/lib/libudev.so.1 $basepath/output-$ARCH/bin/lib
elif [ "$ARCH" == "loongarch64" ]; then
#    cp /opt/boost_1_87_0/lib/libboost_system.so.1.87.0 $basepath/output-$ARCH/bin/lib
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_date_time.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_json.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_thread.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_program_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_atomic.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_container.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp $basepath/output-$ARCH/lib/libhv.so $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
#    cp $basepath/bin/hostEnv.json $basepath/output-$ARCH/bin/
#    cp $basepath/bin/diskStat.json $basepath/output-$ARCH/bin/
#    cp $basepath/bin/tools.iso $basepath/output-$ARCH/bin/
#    cp $basepath/bin/linux-loongarch64/VeraCrypt $basepath/output-$ARCH/bin/ -r
#    cp $basepath/3rd/secDisk/loongarch64/libUSecDiskShare.so $basepath/output-$ARCH/bin/lib/libUSecDisk.so.1.2.0 -r
#    cp /opt/boost_1_87_0/lib/libboost_thread.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/aarch64-linux-gnu/lib/libpcap.so.0.8 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/aarch64-linux-gnu/lib/libsqlite3.so.0 $basepath/output-$ARCH/bin/lib
#    cp /usr/lib/aarch64-linux-gnu/lib/libudev.so.1 $basepath/output-$ARCH/bin/lib
fi
#cp $basepath/out
tar czvf $basepath/moduleCli-${BUILD_NUM}-$ARCH.tar.gz -C $basepath/output-$ARCH .
popd