#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <wlanapi.h>
#include <fcntl.h>
#include <io.h>
#include <locale>
#include <codecvt>
#include "../include/wifi_service.h"
#include "../include/service_installer.h"
#include "../include/network_requester.h"

// 服务名称
const std::wstring SERVICE_NAME = L"WifiAutoConnectService";
// 服务显示名称
const std::wstring DISPLAY_NAME = L"WiFi Auto Connect Service";
// 服务描述
const std::wstring DESCRIPTION = L"Auto connect WiFi service";

// 从注册表读取服务配置
bool ReadServiceConfig(
    const std::wstring& serviceName,
    std::wstring& targetSsid,
    std::wstring& targetPassword,
    std::wstring& loginUsername,
    std::wstring& loginPassword,
    std::wstring& loginUrl,
    std::wstring& campusAccount,
    std::wstring& campusPassword
) {
    // 构建注册表路径
    std::wstring registryPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName + L"\\Parameters";
    
    // 打开注册表项
    HKEY hKey;
    LONG result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        registryPath.c_str(),
        0,
        KEY_READ,
        &hKey
    );
    
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"RegOpenKeyEx失败，错误码: " << result << std::endl;
        return false;
    }
    
    // 读取目标SSID
    wchar_t buffer[1024];
    DWORD bufferSize = sizeof(buffer);
    DWORD type;
    
    result = RegQueryValueExW(
        hKey,
        L"TargetSSID",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        targetSsid = buffer;
    } else {
        std::wcerr << L"读取TargetSSID失败，错误码: " << result << std::endl;
        RegCloseKey(hKey);
        return false;
    }
    
    // 读取目标密码
    bufferSize = sizeof(buffer);
    result = RegQueryValueExW(
        hKey,
        L"TargetPassword",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        targetPassword = buffer;
    } else {
        std::wcerr << L"读取TargetPassword失败，错误码: " << result << std::endl;
        RegCloseKey(hKey);
        return false;
    }
    
    // 读取登录用户名（如果有）
    bufferSize = sizeof(buffer);
    result = RegQueryValueExW(
        hKey,
        L"LoginUsername",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        loginUsername = buffer;
    }
    
    // 读取登录密码（如果有）
    bufferSize = sizeof(buffer);
    result = RegQueryValueExW(
        hKey,
        L"LoginPassword",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        loginPassword = buffer;
    }
    
    // 读取登录URL（如果有）
    bufferSize = sizeof(buffer);
    result = RegQueryValueExW(
        hKey,
        L"LoginURL",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        loginUrl = buffer;
    }
    
    // 读取校园网账号（如果有）
    bufferSize = sizeof(buffer);
    result = RegQueryValueExW(
        hKey,
        L"CampusAccount",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        campusAccount = buffer;
    }
    
    // 读取校园网密码（如果有）
    bufferSize = sizeof(buffer);
    result = RegQueryValueExW(
        hKey,
        L"CampusPassword",
        NULL,
        &type,
        (BYTE*)buffer,
        &bufferSize
    );
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        campusPassword = buffer;
    }
    
    // 关闭注册表项
    RegCloseKey(hKey);
    
    return true;
}

// 打印帮助信息
void PrintHelp() {
    std::wcout << L"WiFi Auto Connect Service\n";
    std::wcout << L"Usage:\n";
    std::wcout << L"  install <SSID> <密码> [登录用户名] [登录密码] [登录URL] [校园网账号] [校园网密码] - 安装服务\n";
    std::wcout << L"  uninstall - 卸载服务\n";
    std::wcout << L"  start - 启动服务\n";
    std::wcout << L"  stop - 停止服务\n";
    std::wcout << L"  status - 查看服务状态\n";
    std::wcout << L"  run <SSID> <密码> [登录用户名] [登录密码] [登录URL] [校园网账号] [校园网密码] - 直接运行（非服务模式）\n";
}

// 获取当前可执行文件路径
std::wstring GetExecutablePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::wstring(path);
}

// 服务入口点
void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    // 从注册表读取配置
    std::wstring targetSsid, targetPassword, loginUsername, loginPassword, loginUrl, campusAccount, campusPassword;
    if (ReadServiceConfig(SERVICE_NAME, targetSsid, targetPassword, loginUsername, loginPassword, loginUrl, campusAccount, campusPassword)) {
        // 创建服务实例
        WifiService service;
        service.SetServiceName(SERVICE_NAME);
        service.SetTargetWifi(targetSsid, targetPassword);
        service.SetLoginCredentials(loginUsername, loginPassword, loginUrl);
        service.SetCampusNetworkCredentials(campusAccount, campusPassword);
        
        // 启动服务
        WifiService::ServiceMain(argc, argv);
    }
}

// 设置控制台以支持中文显示
bool SetupChineseConsole() {
    // 设置控制台代码页为UTF-8
    if (!SetConsoleOutputCP(CP_UTF8)) {
        return false;
    }
    
    // 设置输入代码页为UTF-8
    if (!SetConsoleCP(CP_UTF8)) {
        return false;
    }
    
    // 设置标准输出模式为UTF-8
    _setmode(_fileno(stdout), _O_U8TEXT);
    
    // 获取控制台输出句柄
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // 设置控制台字体为支持中文的字体
    CONSOLE_FONT_INFOEX fontInfo = { sizeof(CONSOLE_FONT_INFOEX) };
    if (!GetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo)) {
        return false;
    }
    
    // 使用支持中文的字体，如新宋体或微软雅黑
    wcscpy_s(fontInfo.FaceName, L"新宋体");
    fontInfo.dwFontSize.Y = 16; // 设置字体大小
    
    if (!SetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo)) {
        return false;
    }
    
    return true;
}

// 将多字节字符串转换为宽字符串
std::wstring CharToWString(const char* str) {
    if (!str) return L"";
    
    // 获取需要的缓冲区大小
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (size_needed <= 0) {
        return L"";
    }
    
    // 分配缓冲区并转换
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstrTo[0], size_needed);
    wstrTo.resize(size_needed - 1);  // 移除结尾的null终止符
    
    return wstrTo;
}

// 主函数
int wmain(int argc, wchar_t* argv[]) {
    try {
        // 设置控制台以支持中文显示
        SetupChineseConsole();
        
        // 如果没有参数，显示帮助信息
        if (argc < 2) {
            PrintHelp();
            return 0;
        }

        std::wstring command = argv[1];

        // 安装服务
        if (command == L"install") {
            if (argc < 4) {
                std::wcout << L"错误: 安装服务需要指定SSID和密码\n";
                PrintHelp();
                return 1;
            }

            std::wstring ssid = argv[2];
            std::wstring password = argv[3];
            std::wstring loginUsername = (argc > 4) ? argv[4] : L"";
            std::wstring loginPassword = (argc > 5) ? argv[5] : L"";
            std::wstring loginUrl = (argc > 6) ? argv[6] : L"";
            std::wstring campusAccount = (argc > 7) ? argv[7] : L"";
            std::wstring campusPassword = (argc > 8) ? argv[8] : L"";

            if (ServiceInstaller::Install(
                SERVICE_NAME, 
                DISPLAY_NAME, 
                DESCRIPTION, 
                GetExecutablePath(),
                ssid,
                password,
                loginUsername,
                loginPassword,
                loginUrl,
                campusAccount,
                campusPassword)) {
                std::wcout << L"服务安装成功\n";
                return 0;
            } else {
                std::wcout << L"服务安装失败\n";
                return 1;
            }
        }
        // 卸载服务
        else if (command == L"uninstall") {
            if (ServiceInstaller::Uninstall(SERVICE_NAME)) {
                std::wcout << L"服务卸载成功\n";
                return 0;
            } else {
                std::wcout << L"服务卸载失败\n";
                return 1;
            }
        }
        // 启动服务
        else if (command == L"start") {
            if (ServiceInstaller::StartServiceImpl(SERVICE_NAME)) {
                std::wcout << L"服务启动成功\n";
                return 0;
            } else {
                std::wcout << L"服务启动失败\n";
                return 1;
            }
        }
        // 停止服务
        else if (command == L"stop") {
            if (ServiceInstaller::StopService(SERVICE_NAME)) {
                std::wcout << L"服务停止成功\n";
                return 0;
            } else {
                std::wcout << L"服务停止失败\n";
                return 1;
            }
        }
        // 查看服务状态
        else if (command == L"status") {
            DWORD status = ServiceInstaller::GetServiceStatus(SERVICE_NAME);
            switch (status) {
                case SERVICE_RUNNING:
                    std::wcout << L"服务状态: 运行中\n";
                    break;
                case SERVICE_STOPPED:
                    std::wcout << L"服务状态: 已停止\n";
                    break;
                case SERVICE_PAUSED:
                    std::wcout << L"服务状态: 已暂停\n";
                    break;
                case SERVICE_START_PENDING:
                    std::wcout << L"服务状态: 正在启动\n";
                    break;
                case SERVICE_STOP_PENDING:
                    std::wcout << L"服务状态: 正在停止\n";
                    break;
                default:
                    std::wcout << L"服务状态: 未知\n";
                    break;
            }
            return 0;
        }
        // 直接运行（非服务模式）
        else if (command == L"run") {
            if (argc < 4) {
                std::wcout << L"错误: 运行需要指定SSID和密码\n";
                PrintHelp();
                return 1;
            }

            std::wstring ssid = argv[2];
            std::wstring password = argv[3];
            std::wstring loginUsername = (argc > 4) ? argv[4] : L"";
            std::wstring loginPassword = (argc > 5) ? argv[5] : L"";
            std::wstring loginUrl = (argc > 6) ? argv[6] : L"";
            std::wstring campusAccount = (argc > 7) ? argv[7] : L"";
            std::wstring campusPassword = (argc > 8) ? argv[8] : L"";

            WifiService service;
            service.SetServiceName(SERVICE_NAME);
            service.SetTargetWifi(ssid, password);
            service.SetLoginCredentials(loginUsername, loginPassword, loginUrl);
            service.SetCampusNetworkCredentials(campusAccount, campusPassword);
            
            if (service.Start()) {
                std::wcout << L"服务已启动，按Ctrl+C停止...\n";
                
                // 等待用户中断
                SetConsoleCtrlHandler([](DWORD ctrlType) -> BOOL {
                    return TRUE;
                }, TRUE);
                
                // 阻塞主线程
                Sleep(INFINITE);
                return 0;
            } else {
                std::wcout << L"服务启动失败\n";
                return 1;
            }
        }
        // 作为服务运行
        else if (command == L"service") {
            SERVICE_TABLE_ENTRYW serviceTable[] = {
                { const_cast<LPWSTR>(SERVICE_NAME.c_str()), ServiceMain },
                { NULL, NULL }
            };

            if (!StartServiceCtrlDispatcherW(serviceTable)) {
                DWORD error = GetLastError();
                if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
                    std::wcout << L"错误: 无法作为服务启动，请使用Service Control Manager启动服务\n";
                } else {
                    std::wcout << L"错误: StartServiceCtrlDispatcher失败，错误码: " << error << L"\n";
                }
                return 1;
            }
            return 0;
        }
        // 未知命令
        else {
            std::wcout << L"未知命令: " << command << L"\n";
            PrintHelp();
            return 1;
        }
    } catch (const std::exception& e) {
        std::wcerr << L"发生异常: " << CharToWString(e.what()) << std::endl;
        return 1;
    } catch (...) {
        std::wcerr << L"发生未知异常" << std::endl;
        return 1;
    }

    return 0;
}