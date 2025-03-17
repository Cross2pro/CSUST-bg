#define UNICODE
#define _UNICODE

#include "../include/wifi_service.h"
#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>

// 静态实例指针初始化
WifiService* WifiService::s_serviceInstance = nullptr;

WifiService::WifiService() : 
    m_serviceStatusHandle(NULL),
    m_serviceStopEvent(NULL),
    m_serviceName(L"WifiAutoConnectService") {
    
    // 初始化服务状态
    ZeroMemory(&m_serviceStatus, sizeof(SERVICE_STATUS));
    m_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_serviceStatus.dwCurrentState = SERVICE_STOPPED;
    m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    m_serviceStatus.dwWin32ExitCode = NO_ERROR;
    m_serviceStatus.dwServiceSpecificExitCode = 0;
    m_serviceStatus.dwCheckPoint = 0;
    m_serviceStatus.dwWaitHint = 0;
    
    // 设置静态实例指针
    s_serviceInstance = this;
}

WifiService::~WifiService() {
    // 确保服务已停止
    Stop();
    
    // 清除静态实例指针
    if (s_serviceInstance == this) {
        s_serviceInstance = nullptr;
    }
    
    // 确保所有资源都被释放
    if (m_serviceStopEvent != NULL) {
        CloseHandle(m_serviceStopEvent);
        m_serviceStopEvent = NULL;
    }
}

bool WifiService::Start() {
    // 确保之前的资源已释放
    Stop();
    
    // 初始化WiFi管理器
    if (!m_wifiManager.Initialize()) {
        std::wcerr << L"WiFi管理器初始化失败" << std::endl;
        return false;
    }
    
    // 初始化网络请求器
    if (!m_networkRequester.Initialize()) {
        std::wcerr << L"网络请求器初始化失败" << std::endl;
        return false;
    }
    
    // 创建服务停止事件
    m_serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_serviceStopEvent == NULL) {
        std::wcerr << L"创建服务停止事件失败，错误码: " << GetLastError() << std::endl;
        return false;
    }
    
    // 创建工作线程
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, this, 0, NULL);
    if (hThread == NULL) {
        std::wcerr << L"创建工作线程失败，错误码: " << GetLastError() << std::endl;
        CloseHandle(m_serviceStopEvent);
        m_serviceStopEvent = NULL;
        return false;
    }
    
    // 关闭线程句柄（线程会继续运行）
    CloseHandle(hThread);
    
    return true;
}

void WifiService::Stop() {
    if (m_serviceStopEvent != NULL) {
        // 设置停止事件
        SetEvent(m_serviceStopEvent);
        
        // 等待工作线程退出，但设置超时以避免无限等待
        Sleep(1000);
        
        // 关闭事件句柄
        CloseHandle(m_serviceStopEvent);
        m_serviceStopEvent = NULL;
    }
}

void WifiService::SetServiceName(const std::wstring& name) {
    m_serviceName = name;
}

void WifiService::SetTargetWifi(const std::wstring& ssid, const std::wstring& password) {
    m_targetSsid = ssid;
    m_targetPassword = password;
}

void WifiService::SetCampusNetworkCredentials(const std::wstring& account, const std::wstring& password) {
    m_campusNetworkAccount = account;
    m_campusNetworkPassword = password;
}

VOID WINAPI WifiService::ServiceMain(DWORD dwArgc, LPWSTR* lpszArgv) {
    // 检查静态实例是否存在
    if (s_serviceInstance == nullptr) {
        return;
    }
    
    // 注册服务控制处理函数
    s_serviceInstance->m_serviceStatusHandle = RegisterServiceCtrlHandlerW(
        (LPCWSTR)s_serviceInstance->m_serviceName.c_str(),
        ServiceCtrlHandler
    );
    
    if (s_serviceInstance->m_serviceStatusHandle == NULL) {
        return;
    }
    
    // 更新服务状态为启动中
    s_serviceInstance->ReportServiceStatus(
        SERVICE_START_PENDING,
        NO_ERROR,
        3000
    );
    
    // 启动服务
    bool result = s_serviceInstance->Start();
    
    // 更新服务状态
    if (result) {
        s_serviceInstance->ReportServiceStatus(
            SERVICE_RUNNING,
            NO_ERROR,
            0
        );
    } else {
        s_serviceInstance->ReportServiceStatus(
            SERVICE_STOPPED,
            ERROR_SERVICE_SPECIFIC_ERROR,
            0
        );
    }
}

VOID WINAPI WifiService::ServiceCtrlHandler(DWORD dwControl) {
    // 检查静态实例是否存在
    if (s_serviceInstance == nullptr) {
        return;
    }
    
    switch (dwControl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        // 更新服务状态为停止中
        s_serviceInstance->ReportServiceStatus(
            SERVICE_STOP_PENDING,
            NO_ERROR,
            0
        );
        
        // 停止服务
        s_serviceInstance->Stop();
        
        // 更新服务状态为已停止
        s_serviceInstance->ReportServiceStatus(
            SERVICE_STOPPED,
            NO_ERROR,
            0
        );
        break;
        
    default:
        break;
    }
}

void WifiService::ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    // 检查是否已注册服务控制处理函数
    if (m_serviceStatusHandle == NULL) {
        return;
    }
    
    // 更新服务状态
    m_serviceStatus.dwCurrentState = dwCurrentState;
    m_serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
    m_serviceStatus.dwWaitHint = dwWaitHint;
    
    // 更新检查点
    if (dwCurrentState == SERVICE_START_PENDING) {
        m_serviceStatus.dwControlsAccepted = 0;
    } else {
        m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }
    
    if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED) {
        m_serviceStatus.dwCheckPoint = 0;
    } else {
        m_serviceStatus.dwCheckPoint++;
    }
    
    // 设置服务状态
    SetServiceStatus(m_serviceStatusHandle, &m_serviceStatus);
}

bool WifiService::PerformCampusNetworkLogin() {
    // 检查是否已连接到WiFi
    if (!m_wifiManager.IsConnected()) {
        std::wcerr << L"未连接到WiFi，无法执行校园网登录" << std::endl;
        return false;
    }
    
    // 检查当前连接的SSID是否为目标SSID
    std::wstring currentSsid = m_wifiManager.GetCurrentSSID();
    if (currentSsid != m_targetSsid) {
        std::wcerr << L"当前连接的WiFi不是目标WiFi，无法执行校园网登录" << std::endl;
        return false;
    }
    
    // 检查校园网账号和密码是否已设置
    if (m_campusNetworkAccount.empty() || m_campusNetworkPassword.empty()) {
        std::wcerr << L"校园网账号或密码未设置,无法执行校园网登录" << std::endl;
        return false;
    }
    
    std::wcout << L"开始执行校园网登录流程..." << std::endl;
    
    // 获取用户IP地址
    std::wstring userIP = m_networkRequester.GetUserIP();
    if (userIP.empty()) {
        std::wcerr << L"获取用户IP地址失败，无法执行校园网登录" << std::endl;
        return false;
    }
    
    // 执行校园网登录
    bool loginResult = m_networkRequester.LoginCampusNetwork(
        m_campusNetworkAccount,
        m_campusNetworkPassword,
        userIP
    );
    
    if (loginResult) {
        std::wcout << L"校园网登录成功" << std::endl;
    } else {
        std::wcerr << L"校园网登录失败" << std::endl;
    }
    
    return loginResult;
}

DWORD WINAPI WifiService::ServiceWorkerThread(LPVOID lpParam) {
    WifiService* service = static_cast<WifiService*>(lpParam);
    
    // 检查参数是否有效
    if (service == nullptr || service->m_serviceStopEvent == NULL) {
        return ERROR_INVALID_PARAMETER;
    }
    
    // 记录上次连接状态，避免重复操作
    bool lastConnected = false;
    std::wstring lastSsid;
    
    // 记录上次校园网登录时间
    ULONGLONG lastLoginAttempt = 0;
    
    // 记录连续失败次数
    DWORD consecutiveFailures = 0;
    
    // 工作循环
    while (WaitForSingleObject(service->m_serviceStopEvent, 0) != WAIT_OBJECT_0) {
        try {
            // 获取当前时间
            ULONGLONG currentTime = GetTickCount64();
            
            // 检查WiFi连接状态
            bool isConnected = service->m_wifiManager.IsConnected();
            std::wstring currentSsid = isConnected ? service->m_wifiManager.GetCurrentSSID() : L"";
            
            // 连接状态发生变化或SSID变化时才执行操作
            if (isConnected != lastConnected || (isConnected && currentSsid != lastSsid)) {
                if (!isConnected) {
                    std::wcout << L"WiFi未连接，尝试连接到: " << service->m_targetSsid << std::endl;
                    
                    // 尝试连接到目标WiFi
                    if (service->m_wifiManager.ConnectToNetwork(service->m_targetSsid, service->m_targetPassword)) {
                        std::wcout << L"WiFi连接成功" << std::endl;
                        
                        // 重置失败计数
                        consecutiveFailures = 0;
                        
                        // 执行校园网登录
                        if (!service->m_campusNetworkAccount.empty() && !service->m_campusNetworkPassword.empty()) {
                            std::wcout << L"尝试校园网登录..." << std::endl;
                            lastLoginAttempt = currentTime;
                            service->PerformCampusNetworkLogin();
                        }
                    } else {
                        std::wcerr << L"WiFi连接失败" << std::endl;
                        consecutiveFailures++;
                        
                        // 根据连续失败次数调整等待时间
                        Sleep(min(consecutiveFailures * 5000, (DWORD)60000)); // 最多等待60秒
                    }
                } else if (currentSsid == service->m_targetSsid) {
                    // 已连接到目标WiFi，检查网络连接状态
                    if (!service->m_networkRequester.CheckNetworkConnection()) {
                        // 如果距离上次登录尝试超过2分钟，再次尝试登录
                        if (currentTime - lastLoginAttempt > 120000) {
                            std::wcout << L"网络连接异常，尝试校园网登录..." << std::endl;
                            lastLoginAttempt = currentTime;
                            service->PerformCampusNetworkLogin();
                        }
                    } else {
                        // 网络连接正常，重置失败计数
                        consecutiveFailures = 0;
                    }
                }
                
                // 更新上次状态
                lastConnected = isConnected;
                lastSsid = currentSsid;
            } else if (isConnected && currentSsid == service->m_targetSsid) {
                // 已连接到目标WiFi，定期检查网络连接状态（每5分钟）
                static ULONGLONG lastNetworkCheck = 0;
                if (currentTime - lastNetworkCheck > 300000) { // 5分钟
                    lastNetworkCheck = currentTime;
                    
                    if (!service->m_networkRequester.CheckNetworkConnection()) {
                        // 如果距离上次登录尝试超过2分钟，再次尝试登录
                        if (currentTime - lastLoginAttempt > 120000) {
                            std::wcout << L"定期检查：网络连接异常，尝试校园网登录..." << std::endl;
                            lastLoginAttempt = currentTime;
                            service->PerformCampusNetworkLogin();
                        }
                    }
                }
            }
            
            // 根据连接状态调整检查频率
            DWORD sleepTime = isConnected ? 30000 : 15000; // 已连接时30秒，未连接时15秒
            
            // 使用可中断的等待，以便能够及时响应停止事件
            WaitForSingleObject(service->m_serviceStopEvent, sleepTime);
        } catch (const std::exception& e) {
            // 捕获并记录异常，防止工作线程崩溃
            std::cerr << "ServiceWorkerThread异常: " << e.what() << std::endl;
            Sleep(10000); // 发生异常后等待10秒再继续
        } catch (...) {
            // 捕获所有未知异常
            std::cerr << "ServiceWorkerThread未知异常" << std::endl;
            Sleep(10000); // 发生异常后等待10秒再继续
        }
    }
    
    return NO_ERROR;
} 