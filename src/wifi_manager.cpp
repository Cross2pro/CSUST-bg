#include "../include/wifi_manager.h"
#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <winhttp.h>
#include <fcntl.h>
#include <io.h>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winhttp.lib")

WifiManager::WifiManager() : m_hClient(NULL) {
}

WifiManager::~WifiManager() {
    Cleanup();
}

bool WifiManager::Initialize() {
    // 确保标准错误输出已设置为UTF-8模式
    static bool stdErrInitialized = false;
    if (!stdErrInitialized) {
        _setmode(_fileno(stderr), _O_U8TEXT);
        stdErrInitialized = true;
    }
    
    // 清理之前的资源
    Cleanup();
    
    // 打开WLAN客户端句柄
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &m_hClient);
    
    if (dwResult != ERROR_SUCCESS) {
        std::wcerr << L"WlanOpenHandle失败，错误码: " << dwResult << std::endl;
        return false;
    }
    
    // 获取接口信息
    return GetInterfaceInfo();
}

bool WifiManager::GetInterfaceInfo() {
    if (m_hClient == NULL) {
        return false;
    }
    
    // 枚举无线接口
    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    DWORD dwResult = WlanEnumInterfaces(m_hClient, NULL, &pIfList);
    
    if (dwResult != ERROR_SUCCESS) {
        std::wcerr << L"WlanEnumInterfaces失败，错误码: " << dwResult << std::endl;
        return false;
    }
    
    // 检查是否有可用的无线接口
    if (pIfList->dwNumberOfItems == 0) {
        std::wcerr << L"没有找到无线网络接口" << std::endl;
        WlanFreeMemory(pIfList);
        return false;
    }
    
    // 使用第一个可用的无线接口
    m_interfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;
    
    // 释放接口列表内存
    WlanFreeMemory(pIfList);
    return true;
}

bool WifiManager::IsConnected() {
    if (m_hClient == NULL) {
        return false;
    }
    
    // 获取连接信息
    PWLAN_CONNECTION_ATTRIBUTES pConnInfo = NULL;
    DWORD dwSize = 0;
    DWORD dwResult = WlanQueryInterface(
        m_hClient,
        &m_interfaceGuid,
        wlan_intf_opcode_current_connection,
        NULL,
        &dwSize,
        (PVOID*)&pConnInfo,
        NULL
    );
    
    if (dwResult != ERROR_SUCCESS) {
        // 如果查询失败，可能是因为没有连接
        return false;
    }
    
    // 检查连接状态
    bool isConnected = (pConnInfo->isState == wlan_interface_state_connected);
    
    // 释放连接信息内存
    WlanFreeMemory(pConnInfo);
    
    return isConnected;
}

std::wstring WifiManager::GetCurrentSSID() {
    if (m_hClient == NULL) {
        return L"";
    }
    
    // 获取连接信息
    PWLAN_CONNECTION_ATTRIBUTES pConnInfo = NULL;
    DWORD dwSize = 0;
    DWORD dwResult = WlanQueryInterface(
        m_hClient,
        &m_interfaceGuid,
        wlan_intf_opcode_current_connection,
        NULL,
        &dwSize,
        (PVOID*)&pConnInfo,
        NULL
    );
    
    if (dwResult != ERROR_SUCCESS) {
        // 如果查询失败，可能是因为没有连接
        return L"";
    }
    
    // 获取SSID
    std::wstring ssid = ConvertSSIDToString(pConnInfo->wlanAssociationAttributes.dot11Ssid);
    
    // 释放连接信息内存
    WlanFreeMemory(pConnInfo);
    
    return ssid;
}

std::vector<std::wstring> WifiManager::GetAvailableNetworks() {
    std::vector<std::wstring> networks;
    
    if (m_hClient == NULL) {
        return networks;
    }
    
    // 扫描无线网络
    DWORD dwResult = WlanScan(m_hClient, &m_interfaceGuid, NULL, NULL, NULL);
    if (dwResult != ERROR_SUCCESS) {
        std::wcerr << L"WlanScan失败，错误码: " << dwResult << std::endl;
        return networks;
    }
    
    // 等待扫描完成
    Sleep(2000);
    
    // 获取可用网络列表
    PWLAN_AVAILABLE_NETWORK_LIST pNetworkList = NULL;
    dwResult = WlanGetAvailableNetworkList(
        m_hClient,
        &m_interfaceGuid,
        WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES |
        WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
        NULL,
        &pNetworkList
    );
    
    if (dwResult != ERROR_SUCCESS) {
        std::wcerr << L"WlanGetAvailableNetworkList失败，错误码: " << dwResult << std::endl;
        return networks;
    }
    
    // 遍历网络列表
    for (DWORD i = 0; i < pNetworkList->dwNumberOfItems; i++) {
        WLAN_AVAILABLE_NETWORK& network = pNetworkList->Network[i];
        std::wstring ssid = ConvertSSIDToString(network.dot11Ssid);
        
        // 避免重复添加相同的SSID
        if (std::find(networks.begin(), networks.end(), ssid) == networks.end()) {
            networks.push_back(ssid);
        }
    }
    
    // 释放网络列表内存
    WlanFreeMemory(pNetworkList);
    
    return networks;
}

bool WifiManager::ConnectToNetwork(const std::wstring& ssid, const std::wstring& password) {
    if (m_hClient == NULL) {
        return false;
    }
    
    // 检查当前连接状态
    if (IsConnected() && GetCurrentSSID() == ssid) {
        std::wcout << L"已经连接到网络: " << ssid << std::endl;
        return true;
    }
    
    // 先执行WiFi扫描，确保能发现目标网络
    std::wcout << L"扫描可用WiFi网络..." << std::endl;
    DWORD scanResult = WlanScan(m_hClient, &m_interfaceGuid, NULL, NULL, NULL);
    if (scanResult != ERROR_SUCCESS) {
        std::wcerr << L"WiFi扫描失败，错误码: " << scanResult << std::endl;
        // 即使扫描失败，也尝试继续连接
    } else {
        // 等待扫描完成
        Sleep(3000);
    }
    
    // 获取指定SSID的网络信息
    PWLAN_AVAILABLE_NETWORK_LIST pNetworkList = NULL;
    DWORD dwResult = WlanGetAvailableNetworkList(
        m_hClient,
        &m_interfaceGuid,
        WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES |
        WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
        NULL,
        &pNetworkList
    );
    
    if (dwResult != ERROR_SUCCESS) {
        std::wcerr << L"WlanGetAvailableNetworkList失败，错误码: " << dwResult << std::endl;
        return false;
    }
    
    // 查找目标SSID的网络
    PWLAN_AVAILABLE_NETWORK pTargetNetwork = NULL;
    for (DWORD i = 0; i < pNetworkList->dwNumberOfItems; i++) {
        WLAN_AVAILABLE_NETWORK& network = pNetworkList->Network[i];
        std::wstring currentSsid = ConvertSSIDToString(network.dot11Ssid);
        
        if (currentSsid == ssid) {
            pTargetNetwork = &network;
            std::wcout << L"找到目标网络: " << ssid << L"，信号强度: " << network.wlanSignalQuality << L"%" << std::endl;
            break;
        }
    }
    
    if (pTargetNetwork == NULL) {
        std::wcerr << L"无法找到SSID为" << ssid << L"的网络，尝试使用通用配置文件" << std::endl;
        WlanFreeMemory(pNetworkList);
        
        // 即使找不到网络，也尝试使用通用配置文件连接
        std::wstring profileXml = CreateProfileXml(ssid, password);
        
        // 设置WiFi配置文件
        DWORD dwReasonCode = 0;
        dwResult = WlanSetProfile(
            m_hClient,
            &m_interfaceGuid,
            0,
            profileXml.c_str(),
            NULL,
            TRUE,
            NULL,
            &dwReasonCode
        );
        
        if (dwResult != ERROR_SUCCESS) {
            std::wcerr << L"WlanSetProfile失败，错误码: " << dwResult << L"，原因码: " << dwReasonCode << std::endl;
            return false;
        }
    } else {
        // 创建WiFi配置文件
        std::wstring profileXml = CreateProfileXml(ssid, password, *pTargetNetwork);
        
        // 释放网络列表内存
        WlanFreeMemory(pNetworkList);
        
        // 设置WiFi配置文件
        DWORD dwReasonCode = 0;
        dwResult = WlanSetProfile(
            m_hClient,
            &m_interfaceGuid,
            0,
            profileXml.c_str(),
            NULL,
            TRUE,
            NULL,
            &dwReasonCode
        );
        
        if (dwResult != ERROR_SUCCESS) {
            std::wcerr << L"WlanSetProfile失败，错误码: " << dwResult << L"，原因码: " << dwReasonCode << std::endl;
            return false;
        }
    }
    
    // 连接到网络
    WLAN_CONNECTION_PARAMETERS params;
    ZeroMemory(&params, sizeof(params));
    params.wlanConnectionMode = wlan_connection_mode_profile;
    params.strProfile = ssid.c_str();
    params.dwFlags = 0;
    params.pDot11Ssid = NULL;
    params.pDesiredBssidList = NULL;
    params.dot11BssType = dot11_BSS_type_infrastructure;
    
    std::wcout << L"尝试连接到WiFi: " << ssid << std::endl;
    dwResult = WlanConnect(
        m_hClient,
        &m_interfaceGuid,
        &params,
        NULL
    );
    
    if (dwResult != ERROR_SUCCESS) {
        std::wcerr << L"WlanConnect失败，错误码: " << dwResult << std::endl;
        return false;
    }
    
    // 等待连接完成，最多等待45秒
    std::wcout << L"等待WiFi连接完成..." << std::endl;
    for (int i = 0; i < 45; i++) {
        Sleep(1000);
        if (IsConnected() && GetCurrentSSID() == ssid) {
            std::wcout << L"成功连接到WiFi: " << ssid << std::endl;
            return true;
        }
        
        // 每5秒显示一次等待状态
        if (i % 5 == 0 && i > 0) {
            std::wcout << L"仍在等待WiFi连接，已等待" << i << L"秒..." << std::endl;
        }
    }
    
    std::wcerr << L"WiFi连接超时" << std::endl;
    return false;
}

void WifiManager::Cleanup() {
    if (m_hClient != NULL) {
        WlanCloseHandle(m_hClient, NULL);
        m_hClient = NULL;
    }
}

std::wstring WifiManager::ConvertSSIDToString(const DOT11_SSID& ssid) {
    if (ssid.uSSIDLength == 0) {
        return L"";
    }
    
    // 将SSID字节转换为宽字符串
    std::wstring result;
    for (ULONG i = 0; i < ssid.uSSIDLength; i++) {
        result += static_cast<wchar_t>(ssid.ucSSID[i]);
    }
    
    return result;
}

std::wstring WifiManager::CreateProfileXml(const std::wstring& ssid, const std::wstring& password) {
    // 调用新的重载函数，使用默认参数
    WLAN_AVAILABLE_NETWORK defaultNetwork = {0};
    defaultNetwork.dot11BssType = dot11_BSS_type_infrastructure;
    
    // 默认使用WPA2PSK和AES
    defaultNetwork.dot11DefaultAuthAlgorithm = DOT11_AUTH_ALGO_RSNA_PSK;
    defaultNetwork.dot11DefaultCipherAlgorithm = DOT11_CIPHER_ALGO_CCMP;
    
    return CreateProfileXml(ssid, password, defaultNetwork);
}

std::wstring WifiManager::CreateProfileXml(const std::wstring& ssid, const std::wstring& password, const WLAN_AVAILABLE_NETWORK& network) {
    std::wstringstream xml;
    
    xml << L"<?xml version=\"1.0\"?>" << std::endl;
    xml << L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">" << std::endl;
    xml << L"    <name>" << ssid << L"</name>" << std::endl;
    xml << L"    <SSIDConfig>" << std::endl;
    xml << L"        <SSID>" << std::endl;
    xml << L"            <name>" << ssid << L"</name>" << std::endl;
    xml << L"        </SSID>" << std::endl;
    xml << L"    </SSIDConfig>" << std::endl;
    
    // 根据网络类型设置connectionType
    xml << L"    <connectionType>";
    switch (network.dot11BssType) {
        case dot11_BSS_type_infrastructure:
            xml << L"ESS";
            break;
        case dot11_BSS_type_independent:
            xml << L"IBSS";
            break;
        case dot11_BSS_type_any:
            xml << L"Any";
            break;
        default:
            xml << L"ESS"; // 默认值
    }
    xml << L"</connectionType>" << std::endl;
    
    xml << L"    <connectionMode>auto</connectionMode>" << std::endl;
    xml << L"    <MSM>" << std::endl;
    xml << L"        <security>" << std::endl;
    xml << L"            <authEncryption>" << std::endl;
    
    // 根据网络认证算法设置
    xml << L"                <authentication>";
    switch (network.dot11DefaultAuthAlgorithm) {
        case DOT11_AUTH_ALGO_80211_OPEN:
            xml << L"open";
            break;
        case DOT11_AUTH_ALGO_80211_SHARED_KEY:
            xml << L"shared";
            break;
        case DOT11_AUTH_ALGO_WPA:
            xml << L"WPA";
            break;
        case DOT11_AUTH_ALGO_WPA_PSK:
            xml << L"WPAPSK";
            break;
        case DOT11_AUTH_ALGO_WPA_NONE:
            xml << L"none";
            break;
        case DOT11_AUTH_ALGO_RSNA:
            xml << L"WPA2";
            break;
        case DOT11_AUTH_ALGO_RSNA_PSK:
            xml << L"WPA2PSK";
            break;
        default:
            xml << L"open"; // 如果未知，则使用开放认证
    }
    xml << L"</authentication>" << std::endl;
    
    // 根据网络加密算法设置
    xml << L"                <encryption>";
    switch (network.dot11DefaultCipherAlgorithm) {
        case DOT11_CIPHER_ALGO_NONE:
            xml << L"none";
            break;
        case DOT11_CIPHER_ALGO_WEP40:
        case DOT11_CIPHER_ALGO_WEP104:
        case DOT11_CIPHER_ALGO_WEP:
            xml << L"WEP";
            break;
        case DOT11_CIPHER_ALGO_TKIP:
            xml << L"TKIP";
            break;
        case DOT11_CIPHER_ALGO_CCMP:
            xml << L"AES";
            break;
        default:
            xml << L"AES"; // 如果未知，则使用AES
    }
    xml << L"</encryption>" << std::endl;
    
    // 根据认证类型决定是否需要OneX
    bool useOneX = (network.dot11DefaultAuthAlgorithm == DOT11_AUTH_ALGO_WPA ||
                   network.dot11DefaultAuthAlgorithm == DOT11_AUTH_ALGO_RSNA);
    
    xml << L"                <useOneX>" << (useOneX ? L"true" : L"false") << L"</useOneX>" << std::endl;
    xml << L"            </authEncryption>" << std::endl;
    
    // 如果需要密码，添加密码信息
    if (!password.empty() && network.dot11DefaultAuthAlgorithm != DOT11_AUTH_ALGO_80211_OPEN) {
        xml << L"            <sharedKey>" << std::endl;
        xml << L"                <keyType>passPhrase</keyType>" << std::endl;
        xml << L"                <protected>false</protected>" << std::endl;
        xml << L"                <keyMaterial>" << password << L"</keyMaterial>" << std::endl;
        xml << L"            </sharedKey>" << std::endl;
    }
    
    xml << L"        </security>" << std::endl;
    xml << L"    </MSM>" << std::endl;
    xml << L"</WLANProfile>";
    
    return xml.str();
} 