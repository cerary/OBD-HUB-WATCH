#pragma once
#include <stdio.h>
#define HOST_LOG(tag, fmt, ...) fprintf(stderr, "%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(...) HOST_LOG(__VA_ARGS__)
#define ESP_LOGW(...) HOST_LOG(__VA_ARGS__)
#define ESP_LOGE(...) HOST_LOG(__VA_ARGS__)
#define ESP_LOGD(...) HOST_LOG(__VA_ARGS__)
