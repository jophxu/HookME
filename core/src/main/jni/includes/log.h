#include <string.h>
#include <android/log.h>

#ifndef HOOKME_LOG_H
#define HOOKME_LOG_H

#define __FILENAME__ (strrchr(__FILE__, '/') + 1)
#define TAG "HookME"

#define LOGD(format, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG, \
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGI(format,...) __android_log_print(ANDROID_LOG_INFO, TAG, \
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGW(format, ...) __android_log_print(ANDROID_LOG_WARN, TAG, \
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, \
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);

#endif //HOOKME_LOG_H
