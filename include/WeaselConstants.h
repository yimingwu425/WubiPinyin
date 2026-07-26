#pragma once

#define WUBIPINYIN_CODE_NAME "WubiPinyin"
#define WUBIPINYIN_PRODUCT_NAME L"\u4e94\u7b14\u00b7\u62fc\u97f3"
#define WUBIPINYIN_REG_KEY L"Software\\Rime\\WubiPinyin"
#define WUBIPINYIN_REG_KEY_32 L"Software\\WOW6432Node\\Rime\\WubiPinyin"
#define WUBIPINYIN_ROOT_VALUE L"WubiPinyinRoot"
#define WUBIPINYIN_TSF_FILE_NAME L"WubiPinyin.dll"
#define WUBIPINYIN_TSF_FILE_NAME_A "WubiPinyin.dll"
#define WUBIPINYIN_SERVER_EXECUTABLE L"WubiPinyinServer.exe"
#define WUBIPINYIN_DEPLOYER_EXECUTABLE L"WubiPinyinDeployer.exe"
#define WUBIPINYIN_SETUP_EXECUTABLE L"WubiPinyinSetup.exe"
#define WUBIPINYIN_SETTINGS_EXECUTABLE L"WubiPinyinSettings.exe"
#define WUBIPINYIN_DEPLOYER_MUTEX L"WubiPinyinDeployerMutex"
#define WUBIPINYIN_DEPLOYER_EXCLUSIVE_MUTEX \
  L"WubiPinyinDeployerExclusiveMutex"
#define WUBIPINYIN_SERVER_MUTEX_PREFIX L"(WubiPinyin)"

// Keep the upstream symbol name while all persisted product identity uses the
// WubiPinyin values above.
#define WEASEL_CODE_NAME WUBIPINYIN_CODE_NAME
#define WEASEL_REG_KEY WUBIPINYIN_REG_KEY

#define STRINGIZE(x) #x
#define VERSION_STR(x) STRINGIZE(x)
#define WEASEL_VERSION VERSION_STR(VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH)
