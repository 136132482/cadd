cmake_minimum_required(VERSION 3.27)
## vcpkg配置，必须位于project之前
#if(DEFINED ENV{VCPKG_ROOT})
#    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "") # 从环境变量中读取vcpkg根目录
#else()
#    set(VCPKG_ROOT "C:\\Users\\Administrator\\CLionProjects\\cadd\\vcpkg" CACHE PATH "") # 手动设置vcpkg根目录
#endif()
#
#message("set VCPKG_ROOT: ${VCPKG_ROOT}")
#set(VCPKG_FEATURE_FLAGS "version") # 用于支持自定义依赖版本
#set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake") # 设置vcpkg工具链


project(cadd)

#include_directories("C:\\Users\\Administrator\\.vcpkg-clion\\vcpkg1\\installed\\x64-windows\\include")
#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#    set(LLVM_CONFIG true)
#    set(CLANG_INCLUDEDIR "D:/Program Files/LLVM/include")
#    set(CLANG_LIBDIR "D:/Program Files/LLVM/lib")
#endif()



# 添加在find_package之前
set(CMAKE_PREFIX_PATH "D:/msys64/mingw64")
set(CMAKE_MODULE_PATH "D:/msys64/mingw64/lib/cmake")
find_package(CURL REQUIRED)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
add_executable(file_main util/fileIo/file_main.cpp  util/fileIo/FileTransferUtility.cpp util/fileIo/FileTransferUtility.h util/Logger/Logger.h util/Logger/Logger.cpp )
# 关键修改4：链接库（必须按此顺序）
target_link_libraries(file_main PRIVATE
        CURL::libcurl
        ws2_32  # Windows sockets
        crypt32  # Crypto API
        ssl      # OpenSSL
        crypto   # OpenSSL
        z        # zlib
)
target_link_libraries(file_main PRIVATE CURL::libcurl)

add_executable(Logger util/Logger/Logger.h util/Logger/Logger.cpp util/Logger/logger_test.cpp )