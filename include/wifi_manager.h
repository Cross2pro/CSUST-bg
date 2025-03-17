#pragma once

#include <windows.h>
#include <wlanapi.h>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    // 初始化WiFi管理器
    bool Initialize();
    
    // 检查WiFi连接状态
    bool IsConnected();
    
    // 获取当前连接的SSID
    std::wstring GetCurrentSSID();
    
    // 获取可用的WiFi网络列表
    std::vector<std::wstring> GetAvailableNetworks();
    
    // 连接到指定SSID的WiFi
    bool ConnectToNetwork(const std::wstring& ssid, const std::wstring& password = L"");

private:
    // WLAN句柄
    HANDLE m_hClient = NULL;
    
    // 接口GUID
    GUID m_interfaceGuid = {};
    
    // 获取接口信息
    bool GetInterfaceInfo();
    
    // 释放资源
    void Cleanup();
    
    // 辅助函数：将SSID字节转换为字符串
    std::wstring ConvertSSIDToString(const DOT11_SSID& ssid);
    
    // 辅助函数：创建WiFi配置文件
    std::wstring CreateProfileXml(const std::wstring& ssid, const std::wstring& password);
    
    // 辅助函数：根据网络信息创建WiFi配置文件
    std::wstring CreateProfileXml(const std::wstring& ssid, const std::wstring& password, const WLAN_AVAILABLE_NETWORK& network);
}; 