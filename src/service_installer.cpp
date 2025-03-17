#define UNICODE
#define _UNICODE

#include "../include/service_installer.h"
#include <windows.h>
#include <iostream>
#include <sstream>

bool ServiceInstaller::Install(
    const std::wstring& serviceName,
    const std::wstring& displayName,
    const std::wstring& description,
    const std::wstring& executablePath,
    const std::wstring& targetSsid,
    const std::wstring& targetPassword,
    const std::wstring& campusAccount,
    const std::wstring& campusPassword
) {
    // 检查服务是否已安装
    if (IsServiceInstalled(serviceName)) {
        std::wcerr << L"服务已存在: " << serviceName << std::endl;
        return false;
    }
    
    // 打开服务控制管理器
    SC_HANDLE schSCManager = OpenSCManager();
    if (schSCManager == NULL) {
        return false;
    }
    
    // 构建服务命令行
    std::wstring commandLine = executablePath + L" service";
    
    // 创建服务
    SC_HANDLE schService = CreateServiceW(
        schSCManager,                   // 服务控制管理器句柄
        serviceName.c_str(),            // 服务名称
        displayName.c_str(),            // 服务显示名称
        SERVICE_ALL_ACCESS,             // 访问权限
        SERVICE_WIN32_OWN_PROCESS,      // 服务类型
        SERVICE_AUTO_START,             // 启动类型
        SERVICE_ERROR_NORMAL,           // 错误控制类型
        commandLine.c_str(),            // 服务二进制文件路径
        NULL,                           // 负载顺序组
        NULL,                           // 标记组内顺序
        NULL,                           // 依赖服务
        NULL,                           // 服务账户
        NULL                            // 服务账户密码
    );
    
    if (schService == NULL) {
        DWORD error = GetLastError();
        std::wcerr << L"CreateService失败，错误码: " << error << std::endl;
        CloseServiceHandle(schSCManager);
        return false;
    }
    
    // 设置服务描述
    SERVICE_DESCRIPTIONW sd;
    sd.lpDescription = const_cast<LPWSTR>(description.c_str());
    
    if (!ChangeServiceConfig2W(
        schService,
        SERVICE_CONFIG_DESCRIPTION,
        &sd
    )) {
        DWORD error = GetLastError();
        std::wcerr << L"ChangeServiceConfig2失败，错误码: " << error << std::endl;
    }
    
    // 关闭服务句柄
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    // 创建服务配置注册表项
    if (!CreateServiceConfigRegistry(
        serviceName,
        targetSsid,
        targetPassword,
        campusAccount,
        campusPassword
    )) {
        std::wcerr << L"创建服务配置注册表项失败" << std::endl;
        // 尝试卸载服务
        Uninstall(serviceName);
        return false;
    }
    
    std::wcout << L"服务安装成功: " << serviceName << std::endl;
    return true;
}

bool ServiceInstaller::Uninstall(const std::wstring& serviceName) {
    // 检查服务是否已安装
    if (!IsServiceInstalled(serviceName)) {
        std::wcerr << L"服务不存在: " << serviceName << std::endl;
        return false;
    }
    
    // 打开服务控制管理器
    SC_HANDLE schSCManager = OpenSCManager();
    if (schSCManager == NULL) {
        return false;
    }
    
    // 打开服务
    SC_HANDLE schService = OpenService(schSCManager, serviceName);
    if (schService == NULL) {
        CloseServiceHandle(schSCManager);
        return false;
    }
    
    // 停止服务
    SERVICE_STATUS serviceStatus;
    if (ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        std::wcout << L"正在停止服务..." << std::endl;
        
        // 等待服务停止
        Sleep(1000);
        
        while (QueryServiceStatus(schService, &serviceStatus)) {
            if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                std::wcout << L"." << std::flush;
                Sleep(1000);
            } else {
                break;
            }
        }
        
        if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
            std::wcout << L"Service stopped" << std::endl;
        } else {
            std::wcout << L"Service could not be stopped" << std::endl;
        }
    }
    
    // 删除服务
    if (!DeleteService(schService)) {
        DWORD error = GetLastError();
        std::wcerr << L"DeleteService失败，错误码: " << error << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }
    
    // 关闭服务句柄
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    // 删除服务配置注册表项
    DeleteServiceConfigRegistry(serviceName);
    
    std::wcout << L"服务卸载成功: " << serviceName << std::endl;
    return true;
}

bool ServiceInstaller::StartServiceImpl(const std::wstring& serviceName) {
    // 检查服务是否已安装
    if (!IsServiceInstalled(serviceName)) {
        std::wcerr << L"服务不存在: " << serviceName << std::endl;
        return false;
    }
    
    // 打开服务控制管理器
    SC_HANDLE schSCManager = OpenSCManager();
    if (schSCManager == NULL) {
        return false;
    }
    
    // 打开服务
    SC_HANDLE schService = OpenService(schSCManager, serviceName);
    if (schService == NULL) {
        CloseServiceHandle(schSCManager);
        return false;
    }
    
    // 启动服务
    if (!::StartService(schService, 0, NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_ALREADY_RUNNING) {
            std::wcout << L"服务已在运行中: " << serviceName << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return true;
        } else {
            std::wcerr << L"StartService失败，错误码: " << error << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }
    }
    
    // 等待服务启动
    SERVICE_STATUS serviceStatus;
    if (QueryServiceStatus(schService, &serviceStatus)) {
        if (serviceStatus.dwCurrentState == SERVICE_START_PENDING) {
            std::wcout << L"正在启动服务..." << std::endl;
            
            while (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_START_PENDING) {
                    std::wcout << L"." << std::flush;
                    Sleep(1000);
                } else {
                    break;
                }
            }
            
            if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
                std::wcout << L"Service started" << std::endl;
            } else {
                std::wcout << L"Service could not be started" << std::endl;
                CloseServiceHandle(schService);
                CloseServiceHandle(schSCManager);
                return false;
            }
        }
    }
    
    // 关闭服务句柄
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    std::wcout << L"服务启动成功: " << serviceName << std::endl;
    return true;
}

bool ServiceInstaller::StopService(const std::wstring& serviceName) {
    // 检查服务是否已安装
    if (!IsServiceInstalled(serviceName)) {
        std::wcerr << L"服务不存在: " << serviceName << std::endl;
        return false;
    }
    
    // 打开服务控制管理器
    SC_HANDLE schSCManager = OpenSCManager();
    if (schSCManager == NULL) {
        return false;
    }
    
    // 打开服务
    SC_HANDLE schService = OpenService(schSCManager, serviceName);
    if (schService == NULL) {
        CloseServiceHandle(schSCManager);
        return false;
    }
    
    // 停止服务
    SERVICE_STATUS serviceStatus;
    if (!ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_NOT_ACTIVE) {
            std::wcout << L"服务未运行: " << serviceName << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return true;
        } else {
            std::wcerr << L"ControlService失败，错误码: " << error << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }
    }
    
    // 等待服务停止
    if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
        std::wcout << L"正在停止服务..." << std::endl;
        
        while (QueryServiceStatus(schService, &serviceStatus)) {
            if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                std::wcout << L"." << std::flush;
                Sleep(1000);
            } else {
                break;
            }
        }
        
        if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
            std::wcout << L"Service stopped" << std::endl;
        } else {
            std::wcout << L"Service could not be stopped" << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }
    }
    
    // 关闭服务句柄
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    std::wcout << L"服务停止成功: " << serviceName << std::endl;
    return true;
}

bool ServiceInstaller::IsServiceInstalled(const std::wstring& serviceName) {
    // 打开服务控制管理器
    SC_HANDLE schSCManager = OpenSCManager();
    if (schSCManager == NULL) {
        return false;
    }
    
    // 打开服务
    SC_HANDLE schService = OpenService(schSCManager, serviceName);
    
    // 关闭服务控制管理器句柄
    CloseServiceHandle(schSCManager);
    
    if (schService == NULL) {
        return false;
    }
    
    // 关闭服务句柄
    CloseServiceHandle(schService);
    
    return true;
}

DWORD ServiceInstaller::GetServiceStatus(const std::wstring& serviceName) {
    // 打开服务控制管理器
    SC_HANDLE schSCManager = OpenSCManager();
    if (schSCManager == NULL) {
        return 0;
    }
    
    // 打开服务
    SC_HANDLE schService = OpenService(schSCManager, serviceName);
    if (schService == NULL) {
        CloseServiceHandle(schSCManager);
        return 0;
    }
    
    // 查询服务状态
    SERVICE_STATUS serviceStatus;
    if (!QueryServiceStatus(schService, &serviceStatus)) {
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return 0;
    }
    
    // 关闭服务句柄
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    return serviceStatus.dwCurrentState;
}

SC_HANDLE ServiceInstaller::OpenSCManager() {
    SC_HANDLE schSCManager = ::OpenSCManager(
        NULL,                   // 本地计算机
        NULL,                   // 活动数据库
        SC_MANAGER_ALL_ACCESS   // 所有访问权限
    );
    
    if (schSCManager == NULL) {
        DWORD error = GetLastError();
        std::wcerr << L"OpenSCManager失败，错误码: " << error << std::endl;
    }
    
    return schSCManager;
}

SC_HANDLE ServiceInstaller::OpenService(SC_HANDLE schSCManager, const std::wstring& serviceName) {
    SC_HANDLE schService = ::OpenServiceW(
        schSCManager,           // 服务控制管理器句柄
        serviceName.c_str(),    // 服务名称
        SERVICE_ALL_ACCESS      // 所有访问权限
    );
    
    if (schService == NULL) {
        DWORD error = GetLastError();
        if (error != ERROR_SERVICE_DOES_NOT_EXIST) {
            std::wcerr << L"OpenService失败，错误码: " << error << std::endl;
        }
    }
    
    return schService;
}

bool ServiceInstaller::CreateServiceConfigRegistry(
    const std::wstring& serviceName,
    const std::wstring& targetSsid,
    const std::wstring& targetPassword,
    const std::wstring& campusAccount,
    const std::wstring& campusPassword
) {
    // 构建注册表路径
    std::wstring registryPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName + L"\\Parameters";
    
    // 创建注册表项
    HKEY hKey;
    DWORD disposition;
    LONG result = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        registryPath.c_str(),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        &disposition
    );
    
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"RegCreateKeyEx失败，错误码: " << result << std::endl;
        return false;
    }
    
    // 设置目标SSID
    result = RegSetValueExW(
        hKey,
        L"TargetSSID",
        0,
        REG_SZ,
        (BYTE*)targetSsid.c_str(),
        (DWORD)((targetSsid.length() + 1) * sizeof(wchar_t))
    );
    
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"RegSetValueEx (TargetSSID) 失败，错误码: " << result << std::endl;
        RegCloseKey(hKey);
        return false;
    }
    
    // 设置目标密码（如果有）
    if (!targetPassword.empty()) {
        result = RegSetValueExW(
            hKey,
            L"TargetPassword",
            0,
            REG_SZ,
            (BYTE*)targetPassword.c_str(),
            (DWORD)((targetPassword.length() + 1) * sizeof(wchar_t))
        );
        
        if (result != ERROR_SUCCESS) {
            std::wcerr << L"RegSetValueEx (TargetPassword) 失败，错误码: " << result << std::endl;
            RegCloseKey(hKey);
            return false;
        }
    }
    
    // 设置校园网账号（如果有）
    if (!campusAccount.empty()) {
        result = RegSetValueExW(
            hKey,
            L"CampusAccount",
            0,
            REG_SZ,
            (BYTE*)campusAccount.c_str(),
            (DWORD)((campusAccount.length() + 1) * sizeof(wchar_t))
        );
        
        if (result != ERROR_SUCCESS) {
            std::wcerr << L"RegSetValueEx (CampusAccount) 失败，错误码: " << result << std::endl;
            RegCloseKey(hKey);
            return false;
        }
    }
    
    // 设置校园网密码（如果有）
    if (!campusPassword.empty()) {
        result = RegSetValueExW(
            hKey,
            L"CampusPassword",
            0,
            REG_SZ,
            (BYTE*)campusPassword.c_str(),
            (DWORD)((campusPassword.length() + 1) * sizeof(wchar_t))
        );
        
        if (result != ERROR_SUCCESS) {
            std::wcerr << L"RegSetValueEx (CampusPassword) 失败，错误码: " << result << std::endl;
            RegCloseKey(hKey);
            return false;
        }
    }
    
    // 关闭注册表句柄
    RegCloseKey(hKey);
    
    return true;
}

bool ServiceInstaller::DeleteServiceConfigRegistry(const std::wstring& serviceName) {
    // 构建注册表路径
    std::wstring registryPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName + L"\\Parameters";
    
    // 删除注册表项
    LONG result = RegDeleteKeyW(HKEY_LOCAL_MACHINE, registryPath.c_str());
    
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        std::wcerr << L"RegDeleteKey失败，错误码: " << result << std::endl;
        return false;
    }
    
    return true;
} 