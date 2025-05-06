cmake_minimum_required(VERSION 3.27)


# 强制使用 vcpkg 工具链（必须在 project() 之前）
set(CMAKE_TOOLCHAIN_FILE "E:/.vcpkg-clion/vcpkg1/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
set(VCPKG_HOST_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++ -D_GLIBCXX_USE_CXX11_ABI=1")

project(cadd)

# 编译器设置（使用 MSYS2 的 Clang）
set(CMAKE_C_COMPILER "D:/msys64/mingw64/bin/clang.exe")
set(CMAKE_CXX_COMPILER "D:/msys64/mingw64/bin/clang++.exe")

# 强制使用 MinGW 的 libstdc++（关键兼容性设置）
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -static-libgcc -static-libstdc++")

# 路径配置（优先级：vcpkg > MSYS2）
list(APPEND CMAKE_PREFIX_PATH
        "E:/.vcpkg-clion/vcpkg1/installed/x64-mingw-dynamic"
        "D:/msys64/mingw64"
)

# 移除可能冲突的 Windows 路径
get_directory_property(inc_dirs INCLUDE_DIRECTORIES)
foreach(dir ${inc_dirs})
    if(dir MATCHES "x64-windows" OR dir MATCHES "Program Files")
        list(REMOVE_ITEM inc_dirs ${dir})
    endif()
endforeach()
set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES "${inc_dirs}")

set(Boost_USE_STATIC_LIBS ON)
set(BOOST_ROOT "E:/.vcpkg-clion/vcpkg1/installed/x64-mingw-dynamic")
find_package(Boost REQUIRED COMPONENTS date_time system)

# 查找所有必需的包
find_package(CURL REQUIRED)
find_package(Drogon REQUIRED COMPONENTS jsoncpp openssl postgresql)
find_package(SOCI REQUIRED COMPONENTS core postgresql)
find_package(PostgreSQL REQUIRED )
find_package(OpenSSL REQUIRED)


# C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加Logger库
add_library(LoggerLib STATIC
        util/Logger/Logger.cpp
        util/Logger/Logger.h
)

# 添加FileTransferUtility库
add_library(FileTransferUtility STATIC
        util/fileIo/FileTransferUtility.cpp
        util/fileIo/FileTransferUtility.h
)

add_library(CryptoUtilLib STATIC
        util/ssl/CryptoUtil.h
        util/ssl/CryptoUtil.cpp  # 添加源文件
)

add_executable(Reflection
        util/register/Reflection.cpp
        util/register/Reflection.h)
add_executable(Reflection_dynamic
        util/register/dynamic_reflection.cpp)
add_executable(duotai duotai.cpp)
add_executable(template template.cpp duotai.cpp)
add_executable(vector vector.cpp)
add_executable(comment_util util/comment_util.cpp util/JsonValueUtil.cpp)
add_executable(JsonValue util/JsonValueUtil.cpp)
add_executable(JsonValueUtil util/JsonValueUtil.cpp util/MyObject.h util/AIGCJson.hpp )
add_executable(ClassRegister util/register/ClassRegister.cpp util/register/base.cpp)
add_executable(test test.cpp  util/AIGCJson.hpp)
add_executable(ReflectionGenerator util/register/ReflectionGenerator.cpp)
add_executable(GenericFactory util/factory/GenericFactory.cpp util/factory/GenericFactory.h util/factory/MathProduct.cpp util/factory/MathProduct.h util/factory/test.cpp
        util/factory/MathProductExtensions.h)


add_executable(CryptoUtil  util/ssl/CryptoUtil.cpp)
target_link_libraries(CryptoUtil PRIVATE
        OpenSSL::Crypto
        CryptoUtilLib
)


# 可执行文件配置示例
add_executable(drogon_test util/drogon/drogon_test.cpp)
target_link_libraries(drogon_test PRIVATE
        Drogon::Drogon
        jsoncpp
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
        ws2_32
        crypt32
        LoggerLib
)

# SOCI数据库应用
add_executable(SociDB util/pgsql/SociDemo.cpp util/pgsql/SociDemo.h util/pgsql/SociDB.h util/pgsql/soci_utils.h)
target_link_libraries(SociDB PRIVATE
        SOCI::soci_core
        SOCI::soci_postgresql
        PostgreSQL::PostgreSQL
        OpenSSL::SSL
        OpenSSL::Crypto
        Boost::date_time
        Boost::system
        ws2_32
        secur32
        CryptoUtilLib
        LoggerLib
)



# 文件传输应用
add_executable(file_main util/fileIo/file_main.cpp)
target_link_libraries(file_main PRIVATE
        FileTransferUtility
        LoggerLib
        CURL::libcurl
        ws2_32
        crypt32
)

# 线程执行器
add_executable(ThreadExecutor util/ThreadExecutor/ThreadExecutor.cpp util/ThreadExecutor/ThreadExecutor.h util/ThreadExecutor/thread_main.cpp)
target_link_libraries(ThreadExecutor PRIVATE
        FileTransferUtility
        LoggerLib
        CURL::libcurl
)





# 其他测试程序
add_executable(logger_test util/Logger/logger_test.cpp)
target_link_libraries(logger_test PRIVATE LoggerLib)

# 通用链接配置（适用于所有目标）
function(configure_common_target target)
    target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
    )
    target_link_options(${target} PRIVATE
            -Wl,--allow-multiple-definition  # 解决符号冲突
            -Wl,--start-group  # 处理循环依赖
    )
    target_link_libraries(${target} PRIVATE
            gdi32  # 图形接口（某些库需要）
            advapi32  # 安全相关
    )
endfunction()

# 应用到所有目标
get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target ${targets})
    if(TARGET ${target} AND NOT target MATCHES "LoggerLib|FileTransferUtility")
        configure_common_target(${target})
    endif()
endforeach()
