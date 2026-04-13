@echo off

:: 检查参数数量
if "%1"=="" (
    echo USAGE: %~nx0 buildNum
    exit /b 1
)

set basepath=%~dp0
cd %basepath%
echo %basepath%

:: 初始化变量
set ARCH=win32
set BUILD_NUM=%1
echo Build number is: %BUILD_NUM%

:: 创建目录
mkdir "%basepath%build-%ARCH%"
mkdir "%basepath%moduleCli-%ARCH%"

@pushd %cd%
setlocal
:: 进入构建目录
cd "%basepath%build-%ARCH%"

:: 执行 CMake 和构建
set BOOST_ROOT=C:\devtool\boost_1_87_0
set MSVC_ROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.29.30133

::set GCCROOT=D:\mingw32
::set GOROOT=D:\Go\sdk\go1.20.14
::set BOOST_ROOT=D:\boost_1_87_0
::set MSVC_ROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.43.34808

cmake -G "Visual Studio 17 2022" -T v142 -A Win32 .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX=%basepath%moduleCli-%ARCH%
if %ERRORLEVEL% NEQ 0 (
    echo CMake 配置失败，退出。
    exit /b %ERRORLEVEL%
)
cmake --build . --config Release --verbose
if %ERRORLEVEL% NEQ 0 (
    echo 构建失败，退出。
    exit /b %ERRORLEVEL%
)
echo 构建成功！

cmake --install . --config Release --prefix "%basepath%moduleCli-%ARCH%"

cd %basepath%

xcopy %basepath%moduleCli-%ARCH%\bin\*  %basepath%moduleCli-%ARCH%\  /y /s
rd /s /q %basepath%moduleCli-%ARCH%\bin

::xcopy %basepath%mediummanager-%ARCH%\lib\*.dll  %basepath%mediummanager-%ARCH%\  /y /s

::xcopy "%basepath%\bin\hostEnv.json" "%basepath%mediummanager-%ARCH%\" /H /Y
::xcopy "%basepath%\bin\diskStat.json" "%basepath%mediummanager-%ARCH%\" /H /Y
::xcopy "%basepath%\bin\tools.iso" "%basepath%mediummanager-%ARCH%\" /H /Y
::xcopy "%basepath%\bin\win32\VeraCrypt\" "%basepath%mediummanager-%ARCH%\VeraCrypt\" /E /I /H /Y
rd /s /q %basepath%moduleCli-%ARCH%\include
rd /s /q %basepath%moduleCli-%ARCH%\lib

::xcopy "%basepath%\3rd\secDisk\win32\SecDiskDLL.dll" "%basepath%mediummanager-%ARCH%\" /E /I /H /Y

endlocal
@popd

echo "zip moduleCli..."
@pushd %cd%
setlocal

cd /d "%basepath%moduleCli-%ARCH%"
"C:\tool\7z.exe" a -tzip -r "..\moduleCli-%BUILD_NUM%-%ARCH%.zip" *
echo "build success!"
endlocal
@popd