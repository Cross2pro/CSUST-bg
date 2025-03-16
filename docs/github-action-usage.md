# GitHub Action 自动发布指南

本项目配置了GitHub Action工作流，可以自动构建项目并发布到GitHub Releases。以下是使用方法：

## 发布新版本

1. 确保你的代码已经准备好发布
2. 创建一个新的标签，标签名称必须以`v`开头，后跟版本号，例如`v1.0.0`：

   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

3. 推送标签后，GitHub Action将自动触发构建流程
4. 构建完成后，会自动创建一个新的Release，并上传构建好的应用程序

## 手动触发构建

本项目还支持手动触发构建流程，无需创建标签。步骤如下：

1. 在GitHub仓库页面，点击"Actions"选项卡
2. 在左侧菜单中选择"手动构建"工作流
3. 点击"Run workflow"按钮
4. 输入版本号（例如：1.0.0）
5. 点击"Run workflow"开始构建

手动触发的构建会创建一个草稿版本的Release，需要手动发布：

1. 构建完成后，在GitHub仓库页面点击"Releases"
2. 找到标记为"Draft"的新Release
3. 检查内容无误后，点击"Publish release"发布

## 工作流程说明

自动发布工作流程包括以下步骤：

1. 检出代码
2. 设置Visual Studio和CMake环境
3. 构建项目
4. 创建发布目录，包含可执行文件和README
5. 创建ZIP压缩包
6. 创建GitHub Release并上传ZIP文件

## 工作流文件说明

本项目包含两个工作流文件：

1. `.github/workflows/build-and-release.yml` - 自动触发的构建和发布流程
   - 当推送带有`v`前缀的标签时触发
   - 自动创建并发布Release

2. `.github/workflows/manual-build.yml` - 手动触发的构建流程
   - 可以通过GitHub Actions页面手动触发
   - 创建草稿版本的Release，需要手动发布

## 注意事项

- 确保项目的CMakeLists.txt文件正确配置
- 发布前请先在本地测试构建是否成功
- 如需修改发布内容，可编辑相应的工作流文件 