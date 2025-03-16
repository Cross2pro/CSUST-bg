#pragma once

#include <windows.h>
#include <string>
#include "wifi_manager.h"
#include "network_requester.h"

class WifiService {
public:
    WifiService();
    ~WifiService();

    // 启动服务
    bool Start();
    
    // 停止服务
    void Stop();
    
    // 设置服务名称
    void SetServiceName(const std::wstring& name);
    
    // 设置目标WiFi信息
    void SetTargetWifi(const std::wstring& ssid, const std::wstring& password);
    
    // 设置自动登录信息
    void SetLoginCredentials(const std::wstring& username, const std::wstring& password, const std::wstring& loginUrl);
    
    // 设置校园网账号信息
    void SetCampusNetworkCredentials(const std::wstring& account, const std::wstring& password);
    
    // 服务主函数
    static VOID WINAPI ServiceMain(DWORD dwArgc, LPWSTR* lpszArgv);
    
    // 服务控制处理函数
    static VOID WINAPI ServiceCtrlHandler(DWORD dwControl);
    
    // 服务状态报告函数
    void ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
    
    // 服务工作线程
    static DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

private:
    // 服务状态
    SERVICE_STATUS m_serviceStatus;
    
    // 服务状态句柄
    SERVICE_STATUS_HANDLE m_serviceStatusHandle;
    
    // 服务停止事件
    HANDLE m_serviceStopEvent;
    
    // 服务名称
    std::wstring m_serviceName;
    
    // 目标WiFi SSID
    std::wstring m_targetSsid;
    
    // 目标WiFi密码
    std::wstring m_targetPassword;
    
    // 登录用户名
    std::wstring m_loginUsername;
    
    // 登录密码
    std::wstring m_loginPassword;
    
    // 登录URL
    std::wstring m_loginUrl;
    
    // 校园网账号
    std::wstring m_campusNetworkAccount;
    
    // 校园网密码
    std::wstring m_campusNetworkPassword;
    
    // WiFi管理器
    WifiManager m_wifiManager;
    
    // 网络请求器
    NetworkRequester m_networkRequester;
    
    // 执行校园网登录
    bool PerformCampusNetworkLogin();
    
    // 静态实例指针（用于回调）
    static WifiService* s_serviceInstance;
}; 