#pragma once

#include <stdbool.h>
#include <string.h>

#define OBD_PROJECT_NAME "OBD HUB WATCH"
#define OBD_PROJECT_CREDITS "Based on SKYGAUGE\nModified & optimized by T A O"
// Compact radio identity fits the existing 12-byte ESP-NOW name field.
#define OBD_MASTER_DEVICE_PREFIX "OBDHUBWatch"
#define OBD_LEGACY_MASTER_DEVICE_PREFIX "SkyGauge"

static inline bool obd_project_is_master_name(const char *name)
{
    return name && (!strncmp(name, OBD_MASTER_DEVICE_PREFIX, sizeof(OBD_MASTER_DEVICE_PREFIX)-1) ||
                    !strncmp(name, OBD_LEGACY_MASTER_DEVICE_PREFIX, sizeof(OBD_LEGACY_MASTER_DEVICE_PREFIX)-1));
}

static inline const char *obd_project_display_master_name(const char *name)
{
    return name && (!strcmp(name, OBD_MASTER_DEVICE_PREFIX) || !strcmp(name, OBD_LEGACY_MASTER_DEVICE_PREFIX)) ?
           OBD_PROJECT_NAME : name;
}
