# WiFi自动连接服务

这是一个Windows服务应用程序，用于自动检测WiFi连接状态，并在断开连接时自动连接到指定的WiFi网络（长理校园网），还可以自动登录校园网。

## 功能特点

- 作为Windows服务在后台运行
- 自动检测WiFi连接状态
- 当WiFi断开时，自动连接到指定的SSID
- 支持自动登录校园网（获取IP地址并执行认证）
- 支持命令行管理（安装、卸载、启动、停止服务）
- 支持直接运行模式（非服务模式）

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
WifiAutoConnectService.exe install <SSID> --password <密码> --ca <校园网账号> --cp <校园网密码>
```

参数说明：
- `<SSID>`: 要连接的WiFi网络名称（如长理校园网）
- `--password <密码>`: WiFi网络密码（如果是开放网络，可以不设置）
- `--ca <校园网账号>`: 校园网认证的用户名
- `--cp <校园网密码>`: 校园网认证的密码

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
WifiAutoConnectService.exe run <SSID> --password <密码> --ca <校园网账号> --cp <校园网密码>
```

## 工作原理

1. 服务启动后，会创建一个工作线程，定期检查WiFi连接状态（约每30秒）
2. 如果检测到WiFi断开连接，会尝试连接到指定的SSID
3. 连接成功后，会自动获取IP地址
4. 获取IP地址后，如果配置了校园网账号和密码，会尝试自动登录校园网
5. 服务会持续监控连接状态，确保WiFi保持连接

## 自动构建与发布

本项目使用GitHub Actions自动构建和发布。可以通过手动触发工作流来构建新版本：

1. 在GitHub仓库页面，点击"Actions"标签
2. 选择"手动构建"工作流
3. 输入版本号（例如：1.0.0）
4. 点击"Run workflow"按钮

构建完成后，会自动创建一个新的Release草稿，包含可执行文件和文档。

## 注意事项

- 服务需要以管理员权限运行
- WiFi密码和校园网登录凭据会存储在Windows注册表中
- 如果需要更改连接的WiFi信息，需要先卸载服务，然后重新安装
- 本服务专为长理校园网环境设计，但也可适用于其他类似的校园网环境

## 许可证

本项目采用MIT许可证。详见LICENSE文件。