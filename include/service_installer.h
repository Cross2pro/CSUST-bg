#pragma once

#include <windows.h>
#include <string>

class ServiceInstaller {
public:
    // 安装服务
    static bool Install(
        const std::wstring& serviceName,
        const std::wstring& displayName,
        const std::wstring& description,
        const std::wstring& executablePath,
        const std::wstring& targetSsid,
        const std::wstring& targetPassword = L"",
        const std::wstring& campusAccount = L"",
        const std::wstring& campusPassword = L""
    );
    
    // 卸载服务
    static bool Uninstall(const std::wstring& serviceName);
    
    // 启动服务
    static bool StartServiceImpl(const std::wstring& serviceName);
    
    // 停止服务
    static bool StopService(const std::wstring& serviceName);
    
    // 检查服务是否已安装
    static bool IsServiceInstalled(const std::wstring& serviceName);
    
    // 获取服务状态
    static DWORD GetServiceStatus(const std::wstring& serviceName);
    
    // 检查服务是否设置为开机自启动
    static bool IsServiceAutoStart(const std::wstring& serviceName);
    
    // 设置服务为开机自启动
    static bool SetServiceAutoStart(const std::wstring& serviceName, bool autoStart);

private:
    // 打开服务控制管理器
    static SC_HANDLE OpenSCManager();
    
    // 打开服务
    static SC_HANDLE OpenService(SC_HANDLE schSCManager, const std::wstring& serviceName);
    
    // 创建服务配置注册表项
    static bool CreateServiceConfigRegistry(
        const std::wstring& serviceName,
        const std::wstring& targetSsid,
        const std::wstring& targetPassword,
        const std::wstring& campusAccount,
        const std::wstring& campusPassword
    );
    
    // 删除服务配置注册表项
    static bool DeleteServiceConfigRegistry(const std::wstring& serviceName);
};