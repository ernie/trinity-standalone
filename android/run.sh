#!/bin/bash

if [ -z "$ANDROID_SDK_ROOT" ]; then
  ANDROID_SDK_ROOT=~/Android/Sdk
fi
if [ -z "$ANDROID_TOOLS_VERSION" ]; then
  ANDROID_TOOLS_VERSION=29.0.2
fi
if [ -z "$KEYSTORE" ]; then
  KEYSTORE=~/.android/release.keystore
fi

SCRIPTDIR=$(readlink -f $(dirname $0))

APP_NAME=$(cat $SCRIPTDIR/app/src/main/res/values/strings.xml | grep "app_name" | cut -d">" -f2 | cut -d"<" -f1 )
APP_VERSION=$(git -C "$SCRIPTDIR" describe --tags --always --dirty 2>/dev/null || echo unknown)
APP_VERSION=${APP_VERSION#v}
APP_PACKAGE=$(cat $SCRIPTDIR/app/src/main/AndroidManifest.xml | grep "package" | cut -d\" -f2 | cut -d\" -f1 )

# NDK paths
NDK_VERSION=21.1.6352462
NDK_PATH=$ANDROID_SDK_ROOT/ndk/$NDK_VERSION
TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake

if [ "$1" == "release" ] || [ "$2" == "release" ] || [ "$3" == "release" ] || [ "$4" == "release" ]; then
  TARGET=release
  GRADLE_BUILD_TYPE=:app:assembleRelease
  APK_NAME=app-release-unsigned.apk
  CMAKE_BUILD_TYPE=Release
else
  TARGET=debug
  GRADLE_BUILD_TYPE=:app:assembleDebug
  APK_NAME=app-debug.apk
  CMAKE_BUILD_TYPE=Debug
fi

APK_LOCATION="$SCRIPTDIR/app/build/outputs/apk/$TARGET"

if [ "$1" == "clean" ] || [ "$2" == "clean" ] || [ "$3" == "clean" ] || [ "$4" == "clean" ]; then
	rm -rf $SCRIPTDIR/../build
	rm -rf $SCRIPTDIR/build
	rm -rf $SCRIPTDIR/app/build
	rm -rf $SCRIPTDIR/app/src/main/jniLibs
fi


cd $SCRIPTDIR/..

# CMake configure (if needed)
NEED_CONFIGURE=0
if [ ! -f "build/CMakeCache.txt" ]; then
    NEED_CONFIGURE=1
    echo "CMakeCache.txt not found, will configure..."
fi

if [ "$NEED_CONFIGURE" -eq 1 ]; then
    echo "Configuring CMake build..."
    cmake -B build -S android/app/src/main/cpp \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-26 \
        -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
        -DFULL_BUILD=ON \
        -G "Ninja"

    if [ $? -ne 0 ]; then
        echo "Failed to configure CMake"
        exit 1
    fi
fi

# CMake build
echo "Building with CMake..."
cmake --build build -j $(getconf _NPROCESSORS_ONLN)

if [ $? -ne 0 ]; then
	echo "Failed to build ioq3"
	exit 1
fi

cd $SCRIPTDIR

./gradlew "$GRADLE_BUILD_TYPE"

if [ $? -ne 0 ]; then
	echo "Failed to build android project"
	exit 1
fi

UNSIGNED_APK="$APK_LOCATION/$APK_NAME"
if [ "$TARGET" == "release" ]; then
  echo "Signing Release APK"
  APK_NAME="$APP_NAME-release-$APP_VERSION.apk"
  SIGNED_APK="$APK_LOCATION/$APK_NAME"
  if [ -z "$KEYSTORE_PASS" ]; then
    echo ""
    echo "Keystore Password:"
    read -s KEYSTORE_PASS
  fi
  apksigner="$ANDROID_SDK_ROOT/build-tools/$ANDROID_TOOLS_VERSION/apksigner"
  $apksigner sign --ks "$KEYSTORE" --out "$SIGNED_APK" --v2-signing-enabled true --ks-pass "pass:$KEYSTORE_PASS" "$UNSIGNED_APK"
  rm "$UNSIGNED_APK"
else
  APK_NAME="$APP_NAME-debug-$APP_VERSION.apk"
  mv "$UNSIGNED_APK" "$APK_LOCATION/$APK_NAME"
fi

if [ "$1" == "install" ] || [ "$2" == "install" ] || [ "$3" == "install" ] || [ "$4" == "install" ]; then
  adb install -r "$APK_LOCATION/$APK_NAME"
  if [ $? -ne 0 ]; then
    adb uninstall $APP_PACKAGE
    adb install "$APK_LOCATION/$APK_NAME"
    if [ $? -ne 0 ]; then
      echo "Failed to install apk."
      exit 1
    fi
  fi
fi

#ANDROID_STORAGE_LOCATION=/sdcard/Android/data/$APP_PACKAGE/files/
#adb shell mkdir -p $ANDROID_STORAGE_LOCATION
#adb push --sync ~/.local/share/Steam/steamapps/common/Quake\ 3\ Arena/baseq3 $ANDROID_STORAGE_LOCATION
#if [ $? -ne 0 ]; then
#	echo "Failed to transfer files."
#	exit 1
#fi
#adb push --sync ../code/renderergl2/glsl $ANDROID_STORAGE_LOCATION/baseq3/
#if [ $? -ne 0 ]; then
#	echo "Failed to transfer shaders."
#	exit 1
#fi
#adb push --sync autoexec.cfg $ANDROID_STORAGE_LOCATION/baseq3/
#if [ $? -ne 0 ]; then
#	echo "Failed to transfer autoexec."
#	exit 1
#fi

if [ "$1" == "start" ] || [ "$2" == "start" ] || [ "$3" == "start" ] || [ "$4" == "start" ]; then
  adb logcat -c
  adb shell am start -n $APP_PACKAGE/.MainActivity
  if [ $? -ne 0 ]; then
    echo "Failed to start application."
    exit 1
  fi
  adb logcat *:S Quake3:V SDL:V DEBUG:V
fi
