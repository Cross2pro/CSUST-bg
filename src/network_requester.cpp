#include "../include/network_requester.h"
#include <iostream>
#include <regex>
#include <sstream>

NetworkRequester::NetworkRequester() : m_hSession(NULL) {
}

NetworkRequester::~NetworkRequester() {
    Cleanup();
}

bool NetworkRequester::Initialize() {
    // 清理之前的资源
    Cleanup();
    
    // 初始化WinHTTP
    m_hSession = WinHttpOpen(
        L"WifiAutoConnectService/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!m_hSession) {
        std::wcerr << L"WinHttpOpen失败，错误码: " << GetLastError() << std::endl;
        return false;
    }
    
    return true;
}

std::wstring NetworkRequester::GetUserIP() {
    std::wcout << L"正在获取用户IP地址..." << std::endl;
    
    try {
        // 发送请求获取IP地址
        std::wstring url = L"https://login.csust.edu.cn/drcom/chkstatus?callback=dr1002&jsVersion=4.X&v=1611&lang=zh";
        std::wstring response = SendHttpGetRequest(url);
        
        if (response.empty()) {
            std::wcerr << L"获取IP地址失败：响应为空" << std::endl;
            return L"";
        }
        
        // 使用正则表达式提取v46ip字段
        std::wregex ipRegex(L"\"v46ip\":\\s*\"([^\"]+)\"");
        std::wsmatch matches;
        
        if (std::regex_search(response, matches, ipRegex) && matches.size() > 1) {
            std::wstring userIP = matches[1].str();
            std::wcout << L"获取到用户IP: " << userIP << std::endl;
            return userIP;
        } else {
            std::wcerr << L"未能从响应中提取IP地址" << std::endl;
            return L"";
        }
    } catch (const std::exception& e) {
        std::wcerr << L"获取IP地址时出错: " << e.what() << std::endl;
        return L"";
    }
}

bool NetworkRequester::LoginCampusNetwork(const std::wstring& account, const std::wstring& password, const std::wstring& userIP) {
    if (userIP.empty()) {
        std::wcerr << L"无法获取用户IP，请检查网络连接" << std::endl;
        return false;
    }
    
    std::wcout << L"\n====================================" << std::endl;
    std::wcout << L"用户IP地址: " << userIP << std::endl;
    std::wcout << L"====================================" << std::endl;
    
    std::wcout << L"开始尝试登录..." << std::endl;
    
    try {
        // 构建登录URL
        std::wstringstream loginUrlStream;
        loginUrlStream << L"https://login.csust.edu.cn:802/eportal/portal/login?callback=dr1003"
                      << L"&login_method=1"
                      << L"&user_account=%2C0%2C" << account
                      << L"&user_password=" << password
                      << L"&wlan_user_ip=" << userIP
                      << L"&wlan_user_ipv6="
                      << L"&wlan_user_mac=000000000000"
                      << L"&wlan_ac_ip="
                      << L"&wlan_ac_name="
                      << L"&jsVersion=4.2.1"
                      << L"&terminal_type=1"
                      << L"&lang=zh-cn"
                      << L"&v=1250"
                      << L"&lang=zh";
        
        std::wstring loginUrl = loginUrlStream.str();
        
        std::wcout << L"使用账号: " << account << std::endl;
        std::wstring response = SendHttpGetRequest(loginUrl);
        
        // 检查登录结果
        if (response.find(L"success") != std::wstring::npos) {
            std::wcout << L"登录请求发送成功" << std::endl;
            
            // 检查网络连接状态
            if (CheckNetworkConnection()) {
                std::wcout << L"\n==============" << std::endl;
                std::wcout << L"     登录成功" << std::endl;
                std::wcout << L"==============" << std::endl;
                return true;
            }
        } else {
            std::wcerr << L"登录请求可能未成功，正在检查网络连接..." << std::endl;
            return CheckNetworkConnection();
        }
    } catch (const std::exception& e) {
        std::wcerr << L"登录过程中出错: " << e.what() << std::endl;
        std::wcerr << L"请检查网络连接" << std::endl;
        return false;
    }
    
    return false;
}

bool NetworkRequester::CheckNetworkConnection() {
    std::wcout << L"检查网络中，请稍后..." << std::endl;
    
    // 定义多个测试网站，提高检测可靠性
    const std::wstring testUrls[] = {
        L"https://www.baidu.com",
        L"https://www.qq.com",
        L"https://www.bing.com"
    };
    
    for (const auto& url : testUrls) {
        try {
            std::wcout << L"尝试访问: " << url << std::endl;
            std::wstring response = SendHttpGetRequest(url);
            if (!response.empty()) {
                std::wcout << L"网络连接正常，可以访问: " << url << std::endl;
                return true;
            }
        } catch (...) {
            // 忽略单个网站的访问异常，继续尝试其他网站
            std::wcout << L"无法访问: " << url << std::endl;
        }
    }
    
    // 尝试访问校园网登录页面
    try {
        std::wstring loginPageUrl = L"https://login.csust.edu.cn/drcom/chkstatus?callback=dr1002&jsVersion=4.X&v=1611&lang=zh";
        std::wstring response = SendHttpGetRequest(loginPageUrl);
        if (!response.empty()) {
            std::wcout << L"可以访问校园网登录页面，但可能需要登录" << std::endl;
            return false; // 可以访问登录页面但不能访问外网，需要登录
        }
    } catch (...) {
        // 忽略异常
    }
    
    std::wcerr << L"断网或者连接失败，无法访问任何测试网站" << std::endl;
    return false;
}

std::wstring NetworkRequester::SendHttpGetRequest(const std::wstring& url, bool isSecure) {
    if (m_hSession == NULL) {
        if (!Initialize()) {
            return L"";
        }
    }
    
    std::wstring hostName, urlPath;
    INTERNET_SCHEME scheme;
    INTERNET_PORT port;
    
    if (!ParseUrl(url, hostName, urlPath, scheme, port)) {
        return L"";
    }
    
    // 连接到服务器
    HINTERNET hConnect = WinHttpConnect(
        m_hSession,
        hostName.c_str(),
        port,
        0
    );
    
    if (!hConnect) {
        std::wcerr << L"WinHttpConnect失败，错误码: " << GetLastError() << std::endl;
        return L"";
    }
    
    // 创建请求
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlPath.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        (scheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0
    );
    
    if (!hRequest) {
        std::wcerr << L"WinHttpOpenRequest失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        return L"";
    }
    
    // 发送请求
    if (!WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    )) {
        std::wcerr << L"WinHttpSendRequest失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return L"";
    }
    
    // 接收响应
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::wcerr << L"WinHttpReceiveResponse失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return L"";
    }
    
    // 读取响应数据
    std::wstring responseData;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer;
    
    do {
        // 检查可用数据大小
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            std::wcerr << L"WinHttpQueryDataAvailable失败，错误码: " << GetLastError() << std::endl;
            break;
        }
        
        // 分配内存
        pszOutBuffer = new char[dwSize + 1];
        if (!pszOutBuffer) {
            std::wcerr << L"内存分配失败" << std::endl;
            break;
        }
        
        // 读取数据
        ZeroMemory(pszOutBuffer, dwSize + 1);
        if (!WinHttpReadData(hRequest, pszOutBuffer, dwSize, &dwDownloaded)) {
            std::wcerr << L"WinHttpReadData失败，错误码: " << GetLastError() << std::endl;
            delete[] pszOutBuffer;
            break;
        }
        
        // 转换为宽字符串并追加到响应数据
        int bufferSize = MultiByteToWideChar(CP_UTF8, 0, pszOutBuffer, dwDownloaded, NULL, 0);
        if (bufferSize > 0) {
            std::wstring wideBuffer(bufferSize, 0);
            MultiByteToWideChar(CP_UTF8, 0, pszOutBuffer, dwDownloaded, &wideBuffer[0], bufferSize);
            responseData += wideBuffer;
        }
        
        // 释放内存
        delete[] pszOutBuffer;
        
    } while (dwSize > 0);
    
    // 关闭句柄
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    
    return responseData;
}

std::wstring NetworkRequester::SendHttpPostRequest(
    const std::wstring& url, 
    const std::wstring& postData, 
    const std::wstring& contentType,
    bool isSecure
) {
    if (m_hSession == NULL) {
        if (!Initialize()) {
            return L"";
        }
    }
    
    std::wstring hostName, urlPath;
    INTERNET_SCHEME scheme;
    INTERNET_PORT port;
    
    if (!ParseUrl(url, hostName, urlPath, scheme, port)) {
        return L"";
    }
    
    // 连接到服务器
    HINTERNET hConnect = WinHttpConnect(
        m_hSession,
        hostName.c_str(),
        port,
        0
    );
    
    if (!hConnect) {
        std::wcerr << L"WinHttpConnect失败，错误码: " << GetLastError() << std::endl;
        return L"";
    }
    
    // 创建请求
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        urlPath.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        (scheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0
    );
    
    if (!hRequest) {
        std::wcerr << L"WinHttpOpenRequest失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        return L"";
    }
    
    // 构建请求头
    std::wstring headers = L"Content-Type: " + contentType;
    
    // 将宽字符串转换为UTF-8
    int postDataSize = WideCharToMultiByte(CP_UTF8, 0, postData.c_str(), -1, NULL, 0, NULL, NULL);
    char* postDataUtf8 = new char[postDataSize];
    WideCharToMultiByte(CP_UTF8, 0, postData.c_str(), -1, postDataUtf8, postDataSize, NULL, NULL);
    
    // 发送请求
    if (!WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        -1,
        (LPVOID)postDataUtf8,
        postDataSize - 1, // 不包括结尾的空字符
        postDataSize - 1,
        0
    )) {
        std::wcerr << L"WinHttpSendRequest失败，错误码: " << GetLastError() << std::endl;
        delete[] postDataUtf8;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return L"";
    }
    
    // 释放POST数据内存
    delete[] postDataUtf8;
    
    // 接收响应
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::wcerr << L"WinHttpReceiveResponse失败，错误码: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return L"";
    }
    
    // 读取响应数据
    std::wstring responseData;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer;
    
    do {
        // 检查可用数据大小
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            std::wcerr << L"WinHttpQueryDataAvailable失败，错误码: " << GetLastError() << std::endl;
            break;
        }
        
        // 分配内存
        pszOutBuffer = new char[dwSize + 1];
        if (!pszOutBuffer) {
            std::wcerr << L"内存分配失败" << std::endl;
            break;
        }
        
        // 读取数据
        ZeroMemory(pszOutBuffer, dwSize + 1);
        if (!WinHttpReadData(hRequest, pszOutBuffer, dwSize, &dwDownloaded)) {
            std::wcerr << L"WinHttpReadData失败，错误码: " << GetLastError() << std::endl;
            delete[] pszOutBuffer;
            break;
        }
        
        // 转换为宽字符串并追加到响应数据
        int bufferSize = MultiByteToWideChar(CP_UTF8, 0, pszOutBuffer, dwDownloaded, NULL, 0);
        if (bufferSize > 0) {
            std::wstring wideBuffer(bufferSize, 0);
            MultiByteToWideChar(CP_UTF8, 0, pszOutBuffer, dwDownloaded, &wideBuffer[0], bufferSize);
            responseData += wideBuffer;
        }
        
        // 释放内存
        delete[] pszOutBuffer;
        
    } while (dwSize > 0);
    
    // 关闭句柄
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    
    return responseData;
}

bool NetworkRequester::ParseUrl(
    const std::wstring& url, 
    std::wstring& hostName, 
    std::wstring& urlPath, 
    INTERNET_SCHEME& scheme, 
    INTERNET_PORT& port
) {
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    
    // 设置要获取的URL组件
    wchar_t hostNameBuffer[256] = {0};
    wchar_t urlPathBuffer[2048] = {0};
    
    urlComp.lpszHostName = hostNameBuffer;
    urlComp.dwHostNameLength = sizeof(hostNameBuffer) / sizeof(hostNameBuffer[0]);
    urlComp.lpszUrlPath = urlPathBuffer;
    urlComp.dwUrlPathLength = sizeof(urlPathBuffer) / sizeof(urlPathBuffer[0]);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwUserNameLength = (DWORD)-1;
    urlComp.dwPasswordLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;
    
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        std::wcerr << L"WinHttpCrackUrl失败，错误码: " << GetLastError() << std::endl;
        return false;
    }
    
    hostName = urlComp.lpszHostName;
    urlPath = urlComp.lpszUrlPath;
    if (urlComp.lpszExtraInfo) {
        urlPath += urlComp.lpszExtraInfo;
    }
    scheme = urlComp.nScheme;
    port = urlComp.nPort;
    
    return true;
}

void NetworkRequester::Cleanup() {
    if (m_hSession != NULL) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = NULL;
    }
} 