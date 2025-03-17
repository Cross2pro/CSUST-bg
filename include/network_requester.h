#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

class NetworkRequester {
public:
    NetworkRequester();
    ~NetworkRequester();

    // 初始化网络请求器
    bool Initialize();

    // 获取用户IP地址
    std::wstring GetUserIP();

    // 登录校园网
    bool LoginCampusNetwork(const std::wstring& account, const std::wstring& password, const std::wstring& userIP);

    // 检查网络连接状态
    bool CheckNetworkConnection();

private:
    // HTTP会话句柄
    HINTERNET m_hSession;

    // 发送HTTP GET请求
    std::wstring SendHttpGetRequest(const std::wstring& url, bool isSecure = true);

    // 发送HTTP POST请求
    std::wstring SendHttpPostRequest(
        const std::wstring& url, 
        const std::wstring& postData, 
        const std::wstring& contentType = L"application/x-www-form-urlencoded",
        bool isSecure = true
    );

    // 解析URL
    bool ParseUrl(
        const std::wstring& url, 
        std::wstring& hostName, 
        std::wstring& urlPath, 
        INTERNET_SCHEME& scheme, 
        INTERNET_PORT& port
    );

    // 清理资源
    void Cleanup();
}; 