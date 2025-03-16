# WiFi自动连接服务

这是一个Windows服务应用程序，用于自动检测WiFi连接状态，并在断开连接时自动连接到指定的WiFi网络，还可以自动登录需要Web认证的网络。

## 功能特点

- 作为Windows服务在后台运行
- 自动检测WiFi连接状态
- 当WiFi断开时，自动连接到指定的SSID
- 支持自动登录需要Web认证的网络（如校园网、酒店WiFi等）
- 支持命令行管理（安装、卸载、启动、停止服务）

## 系统要求

- Windows 7/8/10/11
- 管理员权限（安装和卸载服务需要）
- 支持WiFi的网络适配器
- Visual Studio 2019或更高版本（用于编译）
- CMake 3.10或更高版本

## 编译方法

1. 克隆或下载本项目代码
2. 使用CMake生成项目文件：

```bash
mkdir build
cd build
cmake ..
```

3. 使用Visual Studio或其他编译器编译项目：

```bash
cmake --build . --config Release
```

## 使用方法

### 安装服务

```bash
WifiAutoConnectService.exe install <SSID> <密码> [登录用户名] [登录密码] [登录URL]
```

参数说明：
- `<SSID>`: 要连接的WiFi网络名称
- `<密码>`: WiFi网络密码（如果是开放网络，可以留空）
- `[登录用户名]`: Web认证的用户名（可选）
- `[登录密码]`: Web认证的密码（可选）
- `[登录URL]`: Web认证的登录URL（可选）

### 卸载服务

```bash
WifiAutoConnectService.exe uninstall
```

### 启动服务

```bash
WifiAutoConnectService.exe start
```

### 停止服务

```bash
WifiAutoConnectService.exe stop
```

### 查看服务状态

```bash
WifiAutoConnectService.exe status
```

### 直接运行（非服务模式）

```bash
WifiAutoConnectService.exe run <SSID> <密码> [登录用户名] [登录密码] [登录URL]
```

## 工作原理

1. 服务启动后，会创建一个工作线程，定期检查WiFi连接状态
2. 如果检测到WiFi断开连接，会尝试连接到指定的SSID
3. 连接成功后，如果配置了Web认证信息，会尝试自动登录
4. 服务会每30秒检查一次连接状态

## 注意事项

- 服务需要以管理员权限运行
- WiFi密码和登录凭据会存储在Windows注册表中
- 如果需要更改连接的WiFi信息，需要先卸载服务，然后重新安装

## 许可证

本项目采用MIT许可证。详见LICENSE文件。