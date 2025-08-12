@echo off
REM === 配置区 ===
set QT_DIR=D:\Qt\6.9.1\mingw_64
set APP_NAME=FaeDiag.exe
set PROJECT_DIR=C:\Users\h1283\Desktop\FaeDiag
set OUTPUT_DIR=%PROJECT_DIR%\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\release

REM === 1. 检查可执行文件 ===
if not exist "%OUTPUT_DIR%\%APP_NAME%" (
    echo [错误] 未找到 %OUTPUT_DIR%\%APP_NAME%
    pause
    exit /b
)

REM === 2. 调用 windeployqt 自动复制依赖 ===
echo 正在打包 Qt 依赖...
"%QT_DIR%\bin\windeployqt.exe" --release "%OUTPUT_DIR%\%APP_NAME%"

REM === 3. 复制 adb 工具 ===
echo 复制 ADB 工具...
set ADB_SRC=%PROJECT_DIR%\adb
if exist "%ADB_SRC%\adb.exe" (
    copy "%ADB_SRC%\adb.exe" "%OUTPUT_DIR%\"
    copy "%ADB_SRC%\AdbWinApi.dll" "%OUTPUT_DIR%\"
    copy "%ADB_SRC%\AdbWinUsbApi.dll" "%OUTPUT_DIR%\"
) else (
    echo [警告] 未找到 adb.exe，请确认路径
)

echo -----------------------------------
echo 打包完成！发布目录：
echo %OUTPUT_DIR%
pause
