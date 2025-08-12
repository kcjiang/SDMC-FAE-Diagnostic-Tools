@echo off
REM === ������ ===
set QT_DIR=D:\Qt\6.9.1\mingw_64
set APP_NAME=FaeDiag.exe
set PROJECT_DIR=C:\Users\h1283\Desktop\FaeDiag
set OUTPUT_DIR=%PROJECT_DIR%\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\release

REM === 1. ����ִ���ļ� ===
if not exist "%OUTPUT_DIR%\%APP_NAME%" (
    echo [����] δ�ҵ� %OUTPUT_DIR%\%APP_NAME%
    pause
    exit /b
)

REM === 2. ���� windeployqt �Զ��������� ===
echo ���ڴ�� Qt ����...
"%QT_DIR%\bin\windeployqt.exe" --release "%OUTPUT_DIR%\%APP_NAME%"

REM === 3. ���� adb ���� ===
echo ���� ADB ����...
set ADB_SRC=%PROJECT_DIR%\adb
if exist "%ADB_SRC%\adb.exe" (
    copy "%ADB_SRC%\adb.exe" "%OUTPUT_DIR%\"
    copy "%ADB_SRC%\AdbWinApi.dll" "%OUTPUT_DIR%\"
    copy "%ADB_SRC%\AdbWinUsbApi.dll" "%OUTPUT_DIR%\"
) else (
    echo [����] δ�ҵ� adb.exe����ȷ��·��
)

echo -----------------------------------
echo �����ɣ�����Ŀ¼��
echo %OUTPUT_DIR%
pause
