/* Edge Impulse Espressif ESP32 Standalone Inference ESP IDF Example
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Include ----------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "gas_sensors.h"
#include "my_oled.h"
#include <stdio.h>

#define LED_PIN GPIO_NUM_21

static gas_sensors_config_t sensor_config = {
    .h2s_gpio = 7,
    .odor_gpio = 1,
    .hcho_gpio = 2,
    .nh3_gpio = 5,
    .ethanol_gpio = 6
};

#define NUM_WINDOWS EI_CLASSIFIER_RAW_SAMPLE_COUNT
#define NUM_SENSORS EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
static int feature_index = 0;

void setup_led() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_rom_gpio_pad_select_gpio(LED_PIN);
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    gpio_pad_select_gpio(LED_PIN);
#endif
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

void print_inference_result(ei_impulse_result_t result) {
    // 先输出所有类别的概率
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("%s:%.3f ", ei_classifier_inferencing_categories[i], result.classification[i].value);
    }
    ei_printf("\n");
    
    // 找到概率最高的类别
    float max_value = 0;
    const char* max_label = "";
    
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > max_value) {
            max_value = result.classification[i].value;
            max_label = ei_classifier_inferencing_categories[i];
        }
    }
    
    // 输出格式：result:标签,confidence:概率
    ei_printf("result:%s,confidence:%.3f\n", max_label, max_value);
    fflush(stdout);
}

extern "C" int app_main()
{
    setup_led();
    ei_sleep(100);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化 OLED (使用硬件 I2C)
    oled_init_params_t oled_params = {
        .contrast = 0xFF,
        .lateral_flip = false,   // 不左右翻转
        .longitudinal_flip = false, // 不上下翻转
        .inverse = false
    };
    
    printf("OLED: Initializing with hardware I2C...\n");
    oled_init(&oled_params);
    printf("OLED: Initialized successfully!\n");
    
    // 显示测试
    oled_fill(0x00);
    oled_printf(0, 0, "Fruit Quality", 1);
    oled_printf(0, 24, "Monitoring...", 1);
    oled_refresh();

    if (gas_sensors_init(&sensor_config) != ESP_OK) {
        return 1;
    }

    ei_sleep(2000);

    ei_impulse_result_t result = { nullptr };
    gas_sensors_data_t sensor_data;
    char oled_line[32];

    while (true)
    {
        gpio_set_level(LED_PIN, 1);

        if (gas_sensors_read(&sensor_data) == ESP_OK) {
            // 输出传感器数据
            ei_printf("h2s:%d,odor:%d,hcho:%d,nh3:%d,ethanol:%d\n",
                   sensor_data.h2s_value,
                   sensor_data.odor_value,
                   sensor_data.hcho_value,
                   sensor_data.nh3_value,
                   sensor_data.ethanol_value);
            
            features[feature_index * NUM_SENSORS + 0] = (float)sensor_data.h2s_value;
            features[feature_index * NUM_SENSORS + 1] = (float)sensor_data.odor_value;
            features[feature_index * NUM_SENSORS + 2] = (float)sensor_data.hcho_value;
            features[feature_index * NUM_SENSORS + 3] = (float)sensor_data.nh3_value;
            features[feature_index * NUM_SENSORS + 4] = (float)sensor_data.ethanol_value;

            feature_index = (feature_index + 1) % NUM_WINDOWS;

            if (feature_index == 0) {
                signal_t features_signal;
                features_signal.total_length = sizeof(features) / sizeof(features[0]);
                features_signal.get_data = &raw_feature_get_data;

                EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
                if (res == EI_IMPULSE_OK) {
                    print_inference_result(result);
                    
                    // 更新 OLED 显示
                    float max_value = 0;
                    const char* max_label = "";
                    
                    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                        if (result.classification[i].value > max_value) {
                            max_value = result.classification[i].value;
                            max_label = ei_classifier_inferencing_categories[i];
                        }
                    }
                    
                    oled_fill(0x00);
                    oled_printf(0, 0, "Fruit Quality", 1);
                    
                    sprintf(oled_line, "Result: %s", max_label);
                    oled_printf(0, 20, oled_line, 1);
                    
                    sprintf(oled_line, "Conf: %.1f%%", max_value * 100);
                    oled_printf(0, 40, oled_line, 1);
                    
                    oled_refresh();
                }
            }
        }

        gpio_set_level(LED_PIN, 0);
        ei_sleep(100);
    }
}

