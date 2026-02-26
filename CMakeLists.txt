cmake_minimum_required(VERSION 3.27)


# 强制使用 vcpkg 工具链（必须在 project() 之前）
set(CMAKE_TOOLCHAIN_FILE "E:/.vcpkg-clion/vcpkg1/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
set(VCPKG_HOST_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++ -D_GLIBCXX_USE_CXX11_ABI=1")

# 设置项目根目录宏
add_compile_definitions(CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# 确保可执行文件输出到项目目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

project(cadd)

# 编译器设置（使用 MSYS2 的 Clang）
set(CMAKE_C_COMPILER "D:/msys64/mingw64/bin/clang.exe")
set(CMAKE_CXX_COMPILER "D:/msys64/mingw64/bin/clang++.exe")

# 强制使用 MinGW 的 libstdc++（关键兼容性设置）
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -static-libgcc -static-libstdc++")

include_directories("E:/.vcpkg-clion/vcpkg1/installed/x64-mingw-dynamic/include")
link_directories("E:/.vcpkg-clion/vcpkg1/installed/x64-mingw-dynamic/lib")

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
set(ZMQ_VCPKG_DIR  "E:/.vcpkg-clion/vcpkg1/installed/x64-mingw-dynamic")
set(ZMQ_MSYS2_DIR "D:/msys64/mingw64")


find_package(Boost REQUIRED COMPONENTS date_time system)

# 查找所有必需的包
find_package(hiredis REQUIRED)
find_package(CURL REQUIRED)
find_package(Drogon REQUIRED COMPONENTS jsoncpp openssl postgresql)
find_package(SOCI REQUIRED COMPONENTS core postgresql)
find_package(PostgreSQL REQUIRED )
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system json)
find_package(nlohmann_json REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(cppzmq REQUIRED)
find_package(croncpp REQUIRED)


        # C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(zmqLib STATIC
        util/mq/zmq/ZmqManager.h
        util/mq/zmq/ZmqInstance.h
        util/mq/zmq/ZmqInstance.cpp
)

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

# 声明头文件库 SociLib
add_library(SociLib STATIC
        util/pgsql/SociDB.h
        util/pgsql/soci_utils.h
        util/pgsql/SociDB.cpp
 )

add_library(ApiCommonLib STATIC
        util/drogonweb/api/CallbackNotifier.h
        util/drogonweb/api/LogFilter.h
        util/drogonweb/api/AuthFilter.h
        util/drogonweb/api/Annotations.cpp
)


add_library(HttpClientLib STATIC
        util/http/http_client.h.
        util/http/http_client.cpp
        util/http/FormParser.h
)

# 添加静态库
add_library(ConfigManagerLib STATIC
        config/ConfigManager.h
        config/test_config_main.cpp
)

add_library(RedisUtilLib STATIC
        util/redis/RedisPool.h
        util/redis/RedisPool.cpp
)

add_library(DateUtilLib STATIC
        util/date/DateUtil.h
        util/date/DateUtil.cpp
)

add_library(OrderLib STATIC
        nopeoplecar/OrderCreatorScheduler.h
        nopeoplecar/ParamObj.h
        nopeoplecar/OrderEnums.h
        nopeoplecar/OrderCreatorScheduler.cpp
)
add_library(taskSchedulerLib STATIC
        util/task/TaskScheduler.h
        util/task/TaskScheduler.cpp
)

add_library(enumLib STATIC
        util/enum/EnumSystem.cpp
        util/enum/EnumSystem.h
        util/enum/GenericEnum.cpp
        util/enum/GenericEnum.h
)

add_library(geoLib STATIC
        api/geocode/geoApi.cpp
        api/geocode/geoApi.h
)






add_executable(Reflection
        util/register/Reflection.cpp
        util/register/Reflection.h)
add_executable(Reflection_dynamic
        util/register/dynamic_reflection.cpp)
add_executable(duotai duotai.cpp)
add_executable(template template.cpp duotai.cpp)
add_executable(vector vector.cpp)
add_executable(comment_util util/comment_util.cpp util/json/JsonValueUtil.cpp)
add_executable(JsonValueUtil util/json/JsonValueUtil.cpp util/MyObject.h util/AIGCJson.hpp)
add_executable(ClassRegister util/register/ClassRegister.cpp util/register/base.cpp)
add_executable(test test.cpp util/AIGCJson.hpp)
add_executable(ReflectionGenerator util/register/ReflectionGenerator.cpp)
add_executable(GenericFactory util/factory/GenericFactory.cpp util/factory/GenericFactory.h util/factory/MathProduct.cpp util/factory/MathProduct.h util/factory/test.cpp
        util/factory/MathProductExtensions.h)


add_executable(CryptoUtil util/ssl/CryptoUtil.cpp)
target_link_libraries(CryptoUtil PRIVATE
        OpenSSL::Crypto
        CryptoUtilLib
)

add_executable(Annotations util/drogonweb/api/api_main.cpp util/drogonweb/api/UserController.cpp util/drogonweb/api/UserController.h util/drogonweb/api/Annotations.cpp
        nopeoplecar/OrderService.cpp
)

target_link_libraries(Annotations PRIVATE
        Drogon::Drogon
        jsoncpp
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
        ws2_32
        crypt32
        LoggerLib
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        LoggerLib
        ApiCommonLib
        HttpClientLib
        CURL::libcurl
)



# 可执行文件配置示例
add_executable(drogon_test util/drogonweb/drogon_test.cpp util/drogonweb/DrogonJsonAdapter.h)
target_link_libraries(drogon_test PRIVATE
        Drogon::Drogon
        jsoncpp
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
        ws2_32
        crypt32
        LoggerLib
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
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
        Drogon::Drogon
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

add_executable(http_client util/http/http_client.cpp util/http/http_client.h util/http/http_main.cpp)


add_executable(request_test util/drogonweb/request_test.cpp)
target_link_libraries(request_test PRIVATE
        ${Boost_LIBRARIES}
        pthread
        ws2_32
)


add_executable(JsonConversion util/json/JsonConversion.h util/json/JsonConversion.cpp util/pgsql/SociDemo.h)
target_link_libraries(JsonConversion PRIVATE
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        CryptoUtilLib
)

add_executable(http_main util/drogonweb/api/http_main.cpp)
target_link_libraries(http_main PRIVATE
        HttpClientLib
        CURL::libcurl
        nlohmann_json::nlohmann_json
        CryptoUtilLib
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_executable(crow_main util/crowweb/crow_main.cpp)
target_link_libraries(crow_main PRIVATE
        Crow::Crow
        asio::asio
        ws2_32
        mswsock
        CURL::libcurl
        HttpClientLib
)

add_executable(ws_main util/websocket/ws_main.cpp util/websocket/MyWebSocketClient.h util/websocket/MyWebSocketClient.cpp)
target_link_libraries(crow_main PRIVATE
        Boost::date_time
        Boost::system
        ws2_32
)

add_executable(test_config_main config/test_config_main.cpp)
target_link_libraries(test_config_main
        PRIVATE
        ConfigManagerLib
        nlohmann_json::nlohmann_json
)

add_executable(OrderSubscriber
        nopeoplecar/snatchorders/OrderSubscriber_test.cpp
        nopeoplecar/snatchorders/OrderSubscriber.h
#        util/mq/zmq/ZmqManager.h
#        util/mq/zmq/ZmqInstance.h
#        util/mq/zmq/ZmqInstance.cpp
)
target_link_libraries(OrderSubscriber
        PRIVATE
        RedisUtilLib
        HttpClientLib
        CURL::libcurl
        geoLib
        OrderLib
        DateUtilLib
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        CryptoUtilLib
        taskSchedulerLib
        enumLib
        hiredis::hiredis
        libzmq
        ws2_32
        cppzmq
        zmqLib
)

add_executable(OrderDispatcher
        util/task/PushOrdertaskscheduler_test.cpp
        nopeoplecar/snatchorders/OrderDispatcher.h
        #        nopeoplecar/snatchorders/OrderDispatcher_test.cpp
)

target_link_libraries(OrderDispatcher
        PRIVATE
        RedisUtilLib
        HttpClientLib
        CURL::libcurl
        geoLib
        OrderLib
        DateUtilLib
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        CryptoUtilLib
        taskSchedulerLib
        enumLib
        hiredis::hiredis
        libzmq
        ws2_32
        cppzmq
        zmqLib
)

add_executable(vehicle_thread_stress_test
        nopeoplecar/snatchorders/vehicle_thread_stress_test.cpp
        nopeoplecar/snatchorders/OrderDispatcher.h
)
target_link_libraries(vehicle_thread_stress_test
        PRIVATE
        RedisUtilLib
        HttpClientLib
        CURL::libcurl
        geoLib
        OrderLib
        DateUtilLib
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        CryptoUtilLib
        taskSchedulerLib
        enumLib
        hiredis::hiredis
        libzmq
        ws2_32
        cppzmq
        zmqLib
)


add_executable(zmqpublisher util/mq/zmq/Publisher.cpp util/mq/zmq/ZmqAdvanced.h)
add_executable(zmqsubscriber util/mq/zmq/Subscriber.cpp util/mq/zmq/ZmqAdvanced.h)
# 链接依赖库
target_link_libraries(zmqpublisher PRIVATE
        cppzmq
        libzmq
        ws2_32
)

target_link_libraries(zmqsubscriber PRIVATE
        libzmq
        ws2_32
        cppzmq
        DateUtilLib
)

add_executable(zmqsubscriber1 util/mq/zmq/Subscriber1.cpp util/mq/zmq/ZmqAdvanced.h)
target_link_libraries(zmqsubscriber1
        PRIVATE
        DateUtilLib
        libzmq
        ws2_32
        cppzmq
)

add_executable(zmqsubscriber2 util/mq/zmq/Subscriber2.cpp util/mq/zmq/ZmqAdvanced.h)
target_link_libraries(zmqsubscriber2
        PRIVATE
        DateUtilLib
        libzmq
        ws2_32
        cppzmq
)

add_executable(zmqsubscriber3 util/mq/zmq/Subscriber4.cpp util/mq/zmq/ZmqAdvanced.h)
target_link_libraries(zmqsubscriber3
        PRIVATE
        DateUtilLib
        libzmq
        ws2_32
        cppzmq
)

add_executable(ZmqInstance
        util/mq/zmq/zmq_main.cpp
)
target_link_libraries(ZmqInstance
        PUBLIC
        libzmq
        ws2_32
        cppzmq
        DateUtilLib
        zmqLib
)

add_executable(zmq_test_sub
        util/mq/zmq/zmq_test_sub.cpp
)
target_link_libraries(zmq_test_sub
        PRIVATE
        DateUtilLib
        libzmq
        ws2_32
        cppzmq
        zmqLib
)


add_executable(zmq_test_pub
        util/mq/zmq/zmq_test_pub.cpp
)
target_link_libraries(zmq_test_pub
        PRIVATE
        DateUtilLib
        libzmq
        ws2_32
        cppzmq
        zmqLib
)





add_executable(redis_test util/redis/redis_test.cpp util/redis/RedisPool.h)
target_link_libraries(redis_test PRIVATE
        hiredis::hiredis
)


add_executable(date_test util/date/date_test.cpp)
target_link_libraries(date_test PRIVATE
        DateUtilLib
)

add_executable(taskscheduler_test util/task/taskscheduler_test.cpp)
target_link_libraries(taskscheduler_test PRIVATE
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        taskSchedulerLib
        DateUtilLib
)

add_executable(GenericEnum util/enum/GenericEnum.cpp util/enum/GenericEnum.h util/enum/GenericEnum_test.cpp)

add_executable(EnumSystem util/enum/EnumSystem.cpp util/enum/EnumSystem.h util/enum/EnumSystem_test.cpp)


add_executable(Ordertaskscheduler_test util/task/Ordertaskscheduler_test.cpp)
target_link_libraries(Ordertaskscheduler_test PRIVATE
        RedisUtilLib
        HttpClientLib
        CURL::libcurl
        geoLib
        OrderLib
        DateUtilLib
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        CryptoUtilLib
        taskSchedulerLib
        enumLib
        hiredis::hiredis
)

add_executable(UVehiccletaskscheduler_test util/task/UVehiccletaskscheduler_test.cpp)
target_link_libraries(UVehiccletaskscheduler_test PRIVATE
        RedisUtilLib
        HttpClientLib
        CURL::libcurl
        geoLib
        OrderLib
        DateUtilLib
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        CryptoUtilLib
        taskSchedulerLib
        enumLib
        hiredis::hiredis
)


add_executable(geoApi api/geocode/geoApi.cpp)
target_link_libraries(geoApi PRIVATE
        HttpClientLib
        CURL::libcurl
        nlohmann_json::nlohmann_json
        CryptoUtilLib
        OpenSSL::SSL
        OpenSSL::Crypto
)



add_executable(Orderdeadletter  nopeoplecar/snatchorders/Orderdeadletter.cpp nopeoplecar/snatchorders/Orderdeadletter.h )
target_link_libraries(Orderdeadletter
        PRIVATE
        libzmq
        ws2_32
        cppzmq
        hiredis::hiredis
        DateUtilLib
)

add_executable(OrderDeadLetterTask util/task/OrderDeadLetterTask.cpp)
target_link_libraries(OrderDeadLetterTask PRIVATE
        libzmq
        ws2_32
        cppzmq
        hiredis::hiredis
        LoggerLib
        croncpp::croncpp
        Threads::Threads  # 线程依赖
        taskSchedulerLib
        DateUtilLib
        SociLib
        SOCI::soci_core
        SOCI::soci_postgresql
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        Boost::date_time
        Boost::system
        secur32
        CryptoUtilLib
        Drogon::Drogon
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
























