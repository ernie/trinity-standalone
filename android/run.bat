@echo off

setlocal EnableDelayedExpansion

set BUILD_TYPE=release
set VERSION=unknown
for /f "delims=" %%G in ('git -C "%~dp0." describe --tags --always --dirty 2^>nul') do set "VERSION=%%G"
if "%VERSION:~0,1%"=="v" set "VERSION=%VERSION:~1%"
set BUILD_RENDERER_VK=ON

@REM Define the following environment variables to sign a release build
@REM set KEYSTORE=
@REM set KEYSTORE_PASS=


set ANDROID_SDK_ROOT=%AppData%\..\Local\Android\Sdk
set adb="%ANDROID_SDK_ROOT%\platform-tools\adb.exe"
set apksigner="%ANDROID_SDK_ROOT%\build-tools\29.0.2\apksigner.bat"
set JAVA_HOME=C:\Program Files\Android\Android Studio\jre\jre

@REM NDK paths
set NDK_VERSION=27.3.13750724
set NDK_PATH=%ANDROID_SDK_ROOT%\ndk\%NDK_VERSION%
set TOOLCHAIN_FILE=%NDK_PATH%\build\cmake\android.toolchain.cmake

if "%1"=="clean" (
	rm -rf .\build
	rm -rf .\android\build
	rm -rf .\android\app\src\main\jniLibs\arm64-v8a
)

if %BUILD_TYPE%==release (
	set GRADLE_BUILD_TYPE=:app:assembleRelease
	set CMAKE_BUILD_TYPE=Release
)
if %BUILD_TYPE%==debug (
	set GRADLE_BUILD_TYPE=:app:assembleDebug
	set CMAKE_BUILD_TYPE=Debug
)


pushd %~dp0\..

@REM CMake configure (if needed)
set NEED_CONFIGURE=0
if not exist "build\CMakeCache.txt" (
	set NEED_CONFIGURE=1
	echo CMakeCache.txt not found, will configure...
)

if "!NEED_CONFIGURE!"=="1" (
	echo Configuring CMake build...
	cmake -Wno-deprecated -B build -S android/app/src/main/cpp ^
		-DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" ^
		-DANDROID_ABI=arm64-v8a ^
		-DANDROID_PLATFORM=android-26 ^
		-DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
		-DFULL_BUILD=ON ^
		-G "Ninja"

	if !ERRORLEVEL! NEQ 0 (
		popd
		echo "Failed to configure CMake"
		exit /b 1
	)
)

@REM CMake build
echo Building with CMake...
cmake --build build -j %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
	popd
	echo "Failed to build ioq3"
	exit /b 1
)

pushd android

set GRADLE_EXIT_CONSOLE=1
call gradlew.bat %GRADLE_BUILD_TYPE%

if %ERRORLEVEL% NEQ 0 (
	popd
	popd
	echo "Failed to build android project"
	exit /b 1
)

set PACKAGE_NAME=io.ernie.trinity
set ANDROID_STORAGE_LOCATION=/sdcard/Trinity/
set APK_LOCATION=.\app\build\outputs\apk\%BUILD_TYPE%\trinity-standalone-%BUILD_TYPE%-%VERSION%.apk

if %BUILD_TYPE%==release (
	echo "Signing Release APK"
	call %apksigner% sign --ks ../%KEYSTORE% --out %APK_LOCATION% --v2-signing-enabled true --ks-pass pass:%KEYSTORE_PASS% .\app\build\outputs\apk\%BUILD_TYPE%\app-%BUILD_TYPE%-unsigned.apk
)

if %BUILD_TYPE%==debug (
	echo "Copying Debug APK"
	copy .\app\build\outputs\apk\%BUILD_TYPE%\app-%BUILD_TYPE%.apk %APK_LOCATION%
)

if "%1"=="nodeploy" (
	goto :END
)

%adb% install -r %APK_LOCATION%
if %ERRORLEVEL% NEQ 0 (
	%adb% uninstall %PACKAGE_NAME%
	%adb% install %APK_LOCATION%
	if %ERRORLEVEL% NEQ 0 (
		popd
		popd
		echo "Failed to install apk."
		exit /b 1
	)
)

@REM %adb% shell mkdir -p %ANDROID_STORAGE_LOCATION%
@REM %adb% push --sync "D:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena\baseq3" %ANDROID_STORAGE_LOCATION%
@REM if %ERRORLEVEL% NEQ 0 (
@REM 	popd
@REM 	popd
@REM 	echo "Failed to transfer files."
@REM 	exit /b 1
@REM )

@REM %adb% push --sync ..\code\renderergl2\glsl %ANDROID_STORAGE_LOCATION%/baseq3/
@REM if %ERRORLEVEL% NEQ 0 (
@REM 	popd
@REM 	popd
@REM 	echo "Failed to transfer shaders."
@REM 	exit /b 1
@REM )

@REM %adb% push --sync autoexec.cfg %ANDROID_STORAGE_LOCATION%/baseq3/
@REM if %ERRORLEVEL% NEQ 0 (
@REM 	popd
@REM 	popd
@REM 	echo "Failed to transfer autoexec."
@REM 	exit /b 1
@REM )

%adb% logcat -c
%adb% shell am start -n %PACKAGE_NAME%/.MainActivity
if %ERRORLEVEL% NEQ 0 (
	popd
	popd
	echo "Failed to start application."
	exit 1
)
%adb% logcat *:S Quake3:V SDL:V DEBUG:V OpenXR:V VRVK:V VkValidation:*

:END
endlocal
