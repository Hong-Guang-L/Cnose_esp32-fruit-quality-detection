#include "gas_sensors.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <string.h>

static const char *TAG = "GAS_SENSORS";

// ESP32-S3 GPIO 到 ADC1 通道的映射宏（官方文档）
// GPIO1 → ADC1_CH0
// GPIO2 → ADC1_CH1
// GPIO3 → ADC1_CH2
// GPIO4 → ADC1_CH3
// GPIO5 → ADC1_CH4
// GPIO6 → ADC1_CH5
// GPIO7 → ADC1_CH6
#define GPIO_TO_ADC1_CHANNEL(gpio)  ((gpio == 1) ? ADC_CHANNEL_0 : \
                                     (gpio == 2) ? ADC_CHANNEL_1 : \
                                     (gpio == 3) ? ADC_CHANNEL_2 : \
                                     (gpio == 4) ? ADC_CHANNEL_3 : \
                                     (gpio == 5) ? ADC_CHANNEL_4 : \
                                     (gpio == 6) ? ADC_CHANNEL_5 : \
                                     (gpio == 7) ? ADC_CHANNEL_6 : ADC_CHANNEL_0)

static const char *sensor_names[GAS_SENSOR_COUNT] = {
    "H2S",
    "ODOR",
    "HCHO",
    "NH3",
    "ETHANOL"
};

static int sensor_gpios[GAS_SENSOR_COUNT] = {0};
static bool initialized = false;

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handles[GAS_SENSOR_COUNT] = {NULL};

const gas_sensors_config_t DEFAULT_GAS_SENSORS_CONFIG = {
    .h2s_gpio = 7,
    .odor_gpio = 1,
    .hcho_gpio = 2,
    .nh3_gpio = 5,
    .ethanol_gpio = 6
};

static esp_err_t setup_adc_calibration(adc_unit_t unit, adc_channel_t channel, adc_cali_handle_t *cali_handle)
{
    esp_err_t ret = ESP_FAIL;
    
#if CONFIG_IDF_TARGET_ESP32S3
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, cali_handle);
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration initialized for channel %d", channel);
    } else {
        ESP_LOGW(TAG, "ADC calibration not available for channel %d", channel);
    }
    
    return ret;
}

esp_err_t gas_sensors_init(const gas_sensors_config_t *config)
{
    if (initialized) {
        ESP_LOGW(TAG, "Gas sensors already initialized");
        return ESP_OK;
    }
    
    if (config == NULL) {
        config = &DEFAULT_GAS_SENSORS_CONFIG;
    }
    
    sensor_gpios[GAS_SENSOR_H2S] = config->h2s_gpio;
    sensor_gpios[GAS_SENSOR_ODOR] = config->odor_gpio;
    sensor_gpios[GAS_SENSOR_HCHO] = config->hcho_gpio;
    sensor_gpios[GAS_SENSOR_NH3] = config->nh3_gpio;
    sensor_gpios[GAS_SENSOR_ETHANOL] = config->ethanol_gpio;
    
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    // 根据实际 GPIO 引脚配置对应的 ADC 通道
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
        };
        
        // 根据 GPIO 号计算对应的 ADC1 通道
        adc_channel_t adc_channel = GPIO_TO_ADC1_CHANNEL(sensor_gpios[i]);
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &chan_config));
        
        setup_adc_calibration(ADC_UNIT_1, adc_channel, &adc_cali_handles[i]);
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Gas sensors initialized successfully");
    
    return ESP_OK;
}

esp_err_t gas_sensors_read(gas_sensors_data_t *data)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Gas sensors not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(data, 0, sizeof(gas_sensors_data_t));
    
    int raw_value = 0;
    int voltage_mv = 0;
    
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        // 根据 GPIO 号计算对应的 ADC1 通道
        adc_channel_t adc_channel = GPIO_TO_ADC1_CHANNEL(sensor_gpios[i]);
        
        esp_err_t ret = adc_oneshot_read(adc_handle, adc_channel, &raw_value);
        if (ret == ESP_OK) {
            if (adc_cali_handles[i] != NULL) {
                adc_cali_raw_to_voltage(adc_cali_handles[i], raw_value, &voltage_mv);
            } else {
                voltage_mv = raw_value;
            }
            
            switch (i) {
                case GAS_SENSOR_H2S:
                    data->h2s_value = voltage_mv;
                    break;
                case GAS_SENSOR_ODOR:
                    data->odor_value = voltage_mv;
                    break;
                case GAS_SENSOR_HCHO:
                    data->hcho_value = voltage_mv;
                    break;
                case GAS_SENSOR_NH3:
                    data->nh3_value = voltage_mv;
                    break;
                case GAS_SENSOR_ETHANOL:
                    data->ethanol_value = voltage_mv;
                    break;
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t gas_sensors_read_single(int sensor_type, int *value)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Gas sensors not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sensor_type < 0 || sensor_type >= GAS_SENSOR_COUNT || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int raw_value = 0;
    int voltage_mv = 0;
    
    // 根据 GPIO 号计算对应的 ADC1 通道
    adc_channel_t adc_channel = GPIO_TO_ADC1_CHANNEL(sensor_gpios[sensor_type]);
    
    esp_err_t ret = adc_oneshot_read(adc_handle, adc_channel, &raw_value);
    if (ret == ESP_OK) {
        if (adc_cali_handles[sensor_type] != NULL) {
            adc_cali_raw_to_voltage(adc_cali_handles[sensor_type], raw_value, &voltage_mv);
        } else {
            voltage_mv = raw_value;
        }
        *value = voltage_mv;
    }
    
    return ret;
}

const char* gas_sensors_get_name(int sensor_type)
{
    if (sensor_type < 0 || sensor_type >= GAS_SENSOR_COUNT) {
        return "UNKNOWN";
    }
    return sensor_names[sensor_type];
}

void gas_sensors_deinit(void)
{
    if (!initialized) {
        return;
    }
    
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        if (adc_cali_handles[i] != NULL) {
            adc_cali_delete_scheme_curve_fitting(adc_cali_handles[i]);
            adc_cali_handles[i] = NULL;
        }
    }
    
    if (adc_handle != NULL) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    
    initialized = false;
    ESP_LOGI(TAG, "Gas sensors deinitialized");
}
