#ifndef EDGEHOG_ESP32_DEVICE_DEVMAN_DATA_H
#define EDGEHOG_ESP32_DEVICE_DEVMAN_DATA_H

typedef struct _hardwareInfo {
    char cpu_architecture[32];
    char cpu_model[32];
    char cpu_model_name[32];
    char cpu_vendor[32];
    long mem_total_bytes;
} HardwareInfo;

#endif // EDGEHOG_ESP32_DEVICE_DEVMAN_DATA_H
