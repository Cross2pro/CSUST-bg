#include "../include/wifi_manager.h"
#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <winhttp.h>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winhttp.lib")

WifiManager::WifiManager() : m_hClient(NULL) {
}

WifiManager::~WifiManager() {
    Cleanup();
}

bool WifiManager::Initialize() {
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
    
    // 创建WiFi配置文件
    std::wstring profileXml = CreateProfileXml(ssid, password);
    
    // 设置WiFi配置文件
    DWORD dwReasonCode = 0;
    DWORD dwResult = WlanSetProfile(
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
    
    // 连接到网络
    WLAN_CONNECTION_PARAMETERS params;
    ZeroMemory(&params, sizeof(params));
    params.wlanConnectionMode = wlan_connection_mode_profile;
    params.strProfile = ssid.c_str();
    params.dwFlags = 0;
    params.pDot11Ssid = NULL;
    params.pDesiredBssidList = NULL;
    params.dot11BssType = dot11_BSS_type_infrastructure;
    
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
    
    // 等待连接完成
    for (int i = 0; i < 30; i++) {
        Sleep(1000);
        if (IsConnected() && GetCurrentSSID() == ssid) {
            std::wcout << L"成功连接到网络: " << ssid << std::endl;
            return true;
        }
    }
    
    std::wcerr << L"连接超时" << std::endl;
    return false;
}

bool WifiManager::AutoLogin(const std::wstring& username, const std::wstring& password, const std::wstring& loginUrl) {
    // 如果没有提供登录URL，则不需要自动登录
    if (loginUrl.empty()) {
        return true;
    }
    
    // 检查是否已连接到网络
    if (!IsConnected()) {
        std::wcerr << L"Not connected to network, cannot auto login." << std::endl;
        return false;
    }
    
    // 初始化WinHTTP
    HINTERNET hSession = WinHttpOpen(
        L"WifiAutoConnectService/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!hSession) {
        std::wcerr << L"WinHttpOpen失败，错误码: " << GetLastError() << std::endl;
        return false;
    }
    
    // 解析URL
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    
    // 设置要获取的URL组件
    wchar_t hostName[256] = {0};
    wchar_t urlPath[1024] = {0};
    
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName) / sizeof(hostName[0]);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(urlPath[0]);
    urlComp.dwSchemeLength = -1;
    urlComp.dwUserNameLength = -1;
    urlComp.dwPasswordLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;
    
    if (!WinHttpCrackUrl(loginUrl.c_str(), (DWORD)loginUrl.length(), 0, &urlComp)) {
        std::wcerr << L"WinHttpCrackUrl失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // 连接到服务器
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        urlComp.lpszHostName,
        urlComp.nPort,
        0
    );
    
    if (!hConnect) {
        std::wcerr << L"WinHttpConnect失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // 创建请求
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        urlComp.lpszUrlPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0
    );
    
    if (!hRequest) {
        std::wcerr << L"WinHttpOpenRequest失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // 构建POST数据
    std::wstring postData = L"username=" + username + L"&password=" + password;
    
    // 发送请求
    if (!WinHttpSendRequest(
        hRequest,
        L"Content-Type: application/x-www-form-urlencoded",
        -1,
        (LPVOID)postData.c_str(),
        (DWORD)(postData.length() * sizeof(wchar_t)),
        (DWORD)(postData.length() * sizeof(wchar_t)),
        0
    )) {
        std::wcerr << L"WinHttpSendRequest失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // 接收响应
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::wcerr << L"WinHttpReceiveResponse失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // 获取HTTP状态码
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    
    if (!WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX
    )) {
        std::wcerr << L"WinHttpQueryHeaders失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // 关闭句柄
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    // 检查状态码
    if (statusCode == 200 || statusCode == 302) {
        std::wcout << L"自动登录成功" << std::endl;
        return true;
    } else {
        std::wcerr << L"自动登录失败，HTTP状态码: " << statusCode << std::endl;
        return false;
    }
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
    std::wstringstream xml;
    
    xml << L"<?xml version=\"1.0\"?>" << std::endl;
    xml << L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">" << std::endl;
    xml << L"    <name>" << ssid << L"</name>" << std::endl;
    xml << L"    <SSIDConfig>" << std::endl;
    xml << L"        <SSID>" << std::endl;
    xml << L"            <name>" << ssid << L"</name>" << std::endl;
    xml << L"        </SSID>" << std::endl;
    xml << L"    </SSIDConfig>" << std::endl;
    xml << L"    <connectionType>ESS</connectionType>" << std::endl;
    xml << L"    <connectionMode>auto</connectionMode>" << std::endl;
    xml << L"    <MSM>" << std::endl;
    xml << L"        <security>" << std::endl;
    
    if (password.empty()) {
        // 开放网络
        xml << L"            <authEncryption>" << std::endl;
        xml << L"                <authentication>open</authentication>" << std::endl;
        xml << L"                <encryption>none</encryption>" << std::endl;
        xml << L"                <useOneX>false</useOneX>" << std::endl;
        xml << L"            </authEncryption>" << std::endl;
    } else {
        // WPA/WPA2网络
        xml << L"            <authEncryption>" << std::endl;
        xml << L"                <authentication>WPA2PSK</authentication>" << std::endl;
        xml << L"                <encryption>AES</encryption>" << std::endl;
        xml << L"                <useOneX>false</useOneX>" << std::endl;
        xml << L"            </authEncryption>" << std::endl;
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