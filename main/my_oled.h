#ifndef __MY_OLED_H_
#define __MY_OLED_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/task.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_I2C_SDA_IO 40          //I2C SDA GPIO Number   
#define OLED_I2C_SCL_IO 39          //I2C SCL GPIO Number    
#define I2C_MASTER_NUM 0           //OLED 使用的 I2C 端口号
#define OLED_I2C_FREQ 400000        //OLED 对应 I2C 时钟  
#define OLED_ADDR 0x78              //OLED I2C 设备地址    

#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TX_BUF_DISABLE 0

typedef struct {
    uint8_t contrast;     
    bool lateral_flip;   
    bool longitudinal_flip;
    bool inverse;  
} oled_init_params_t;

extern uint8_t ROM[8][128];

void oled_refresh(void);

esp_err_t iic_init(void);
void iic_sendData(uint8_t HWaddress, uint8_t RGaddress, uint8_t data);

void oled_init(oled_init_params_t * oled_params);
void oled_sendCom(uint8_t Com);
void oled_sendData(uint8_t *Dat, uint16_t size);
void oled_fill(uint8_t data);

void oled_point(uint8_t x, uint8_t y, bool cover);
void oled_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool cover);
void oled_rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool cover);
void oled_printf(uint8_t x1, uint8_t y1, char *string, bool cover);

void oled_display_off(void);
void oled_display_on(void);

#ifdef __cplusplus
}
#endif

#endif
