cmake_minimum_required(VERSION 3.10)

project(WifiAutoConnectService VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加源文件
add_executable(WifiAutoConnectService 
    src/main.cpp
    src/wifi_service.cpp
    src/wifi_manager.cpp
    src/service_installer.cpp
    src/network_requester.cpp
)

# 包含头文件目录
include_directories(include)

# 链接Windows库
target_link_libraries(WifiAutoConnectService 
    wlanapi
    advapi32
    ws2_32
)

# 安装目标
install(TARGETS WifiAutoConnectService DESTINATION bin) 