/**
 * hal_gpio.h — Hardware Abstraction Layer: GPIO
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF v5.x.
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/. Upper layers reach hardware
 * exclusively through the platform/ services layer.
 *
 * NOT host-compilable — depends on ESP-IDF driver headers (added in later phases).
 */

#ifndef FIESTAQUEST_HAL_GPIO_H
#define FIESTAQUEST_HAL_GPIO_H

#include <stdint.h>

/* Placeholder: expanded in later phases when ESP-IDF headers are available. */

#endif /* FIESTAQUEST_HAL_GPIO_H */
