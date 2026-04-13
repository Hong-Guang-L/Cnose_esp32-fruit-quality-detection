#ifndef _GAS_SENSORS_H_
#define _GAS_SENSORS_H_

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAS_SENSOR_H2S      0
#define GAS_SENSOR_ODOR     1
#define GAS_SENSOR_HCHO     2
#define GAS_SENSOR_NH3      3
#define GAS_SENSOR_ETHANOL  4
#define GAS_SENSOR_COUNT    5

typedef struct {
    int h2s_value;
    int odor_value;
    int hcho_value;
    int nh3_value;
    int ethanol_value;
} gas_sensors_data_t;

typedef struct {
    int h2s_gpio;
    int odor_gpio;
    int hcho_gpio;
    int nh3_gpio;
    int ethanol_gpio;
} gas_sensors_config_t;

extern const gas_sensors_config_t DEFAULT_GAS_SENSORS_CONFIG;

esp_err_t gas_sensors_init(const gas_sensors_config_t *config);

esp_err_t gas_sensors_read(gas_sensors_data_t *data);

esp_err_t gas_sensors_read_single(int sensor_type, int *value);

const char* gas_sensors_get_name(int sensor_type);

void gas_sensors_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
