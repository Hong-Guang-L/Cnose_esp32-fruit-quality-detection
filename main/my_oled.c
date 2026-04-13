#include "my_oled.h"
#include "my_codetab.h"

uint8_t ROM[8][128];

esp_err_t iic_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_I2C_SDA_IO,
        .scl_io_num = OLED_I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = OLED_I2C_FREQ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void iic_sendData(uint8_t HWaddress, uint8_t RGaddress, uint8_t data)
{
    uint8_t data_buf[2]={RGaddress, data};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, HWaddress & 0xFE, 1);
    i2c_master_write(cmd, data_buf, sizeof(data_buf), 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);    
}

void oled_printf(uint8_t x1, uint8_t y1, char *string, bool cover)
{
    if (x1 > 127 || y1 > 63) {
        printf("Error:x,y range error 0<x<128 0<y<64");
        return;
    }
    
    uint8_t lower_column = (y1 / 8);
    uint8_t len = 0;
    
    while (*string != '\0') {
        for (uint8_t i = lower_column; i <= lower_column + 2; i++) {	
            if (i == lower_column) { 
                for (uint8_t j = 0; j < 8; j++) {
                    if (cover) ROM[i][x1 + 8 * len + j] = (str_code12_24[(uint8_t)(*string) - 32][j] << (y1 % 8));
                    else ROM[i][x1 + 8 * len + j] |= (str_code12_24[(uint8_t)(*string) - 32][j] << (y1 % 8));
                }
            } else if (i == (lower_column + 1)) { 
                for (uint8_t j = 0; j < 8; j++) {
                    if (cover) ROM[i][x1 + 8 * len + j] = ((str_code12_24[(uint8_t)(*string) - 32][j] >> (8 - (y1 % 8))) |
                        (str_code12_24[(uint8_t)(*string) - 32][j + 8] << ((y1 + 8) % 8)));
                    else ROM[i][x1 + 8 * len + j] |= ((str_code12_24[(uint8_t)(*string) - 32][j] >> (8 - (y1 % 8))) |
                        (str_code12_24[(uint8_t)(*string) - 32][j + 8] << ((y1 + 8) % 8)));
                }
            } else {
                for (uint8_t j = 0; j < 8; j++) {
                    if (cover) ROM[i][x1 + 8 * len + j] = (str_code12_24[(uint8_t)(*string) - 32][j + 8] >> (8 - (y1 + 16) % 8));
                    else ROM[i][x1 + 8 * len + j] |= (str_code12_24[(uint8_t)(*string) - 32][j + 8] >> (8 - (y1 + 16) % 8));
                }				
            }
        }
        len++;
        string++;	
    }	
}

void oled_point(uint8_t x, uint8_t y, bool cover) 
{
    if (x > 127 || y > 63) {
        printf("Error:x %d,y %d,range error 0<x<128 0<y<64\n", x, y);
        return;
    }
    if (cover) ROM[y / 8][x] = (1 << (y % 8));
    else ROM[y / 8][x] |= (1 << (y % 8));
}

void oled_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool cover)
{
    uint8_t max_x, max_y, min_x, min_y;
    
    if (x1 > x2) { max_x = x1; min_x = x2; }
    else { max_x = x2; min_x = x1; }
    if (y1 > y2) { max_y = y1; min_y = y2; }
    else { max_y = y2; min_y = y1; }
	
    if (max_x > 127 || max_y > 63) {
        printf("Error:x %d,y %d,range error 0<x<128 0<y<64\n", max_x, max_y);
        return;
    }

    if (x1 == x2) {
        for (uint8_t i = min_y; i < max_y; i++) {
            oled_point(x1, i, cover);
        }	
    } else if (y1 == y2) {
        for (uint8_t i = min_x; i < max_x; i++) {
            oled_point(i, y1, cover);
        }	
    } else {
        float tan_val = ((float)(y2 - y1)) / ((float)(x2 - x1));
        if (tan_val > 0) {
            for (uint8_t i = min_x; i < max_x; i++) {
                oled_point(i, min_y + (int)(tan_val * (i - min_x)), cover);
            }
        } else {
            for (uint8_t i = min_x; i < max_x; i++) {
                oled_point(i, max_y + (int)(tan_val * (i - min_x)), cover);
            }
        }
    }
}

void oled_rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool cover)
{
    oled_line(x1, y1, x2, y1, cover);
    oled_line(x1, y2, x2, y2, cover);
    oled_line(x1, y1, x1, y2, cover);
    oled_line(x2, y1, x2, y2, cover);
}

void oled_init(oled_init_params_t *oled_params)
{
    iic_init();
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    oled_sendCom(0xAE); //display off
    oled_sendCom(0x20); //Set Memory Addressing Mode
    oled_sendCom(0x10); //Page Addressing Mode
    oled_sendCom(0xb0); //Set Page Start Address
    oled_sendCom(0x00); //set low column address
    oled_sendCom(0x10); //set high column address
    oled_sendCom(0x40); //set start line address
    oled_sendCom(0x81); //set contrast control register
    oled_sendCom(oled_params->contrast); //set contrast
    
    if (oled_params->longitudinal_flip) oled_sendCom(0xc0); //flip c0/c8
    else oled_sendCom(0xc8);
    
    if (oled_params->lateral_flip) oled_sendCom(0xa0); //flip a0/a1
    else oled_sendCom(0xa1);
    
    if (oled_params->inverse) oled_sendCom(0xa7);
    else oled_sendCom(0xa6); //normal display
    
    oled_sendCom(0xa8); //set multiplex ratio
    oled_sendCom(0x3F); //
    oled_sendCom(0xa4); //Output follows RAM content
    oled_sendCom(0xd3); //set display offset
    oled_sendCom(0x00); //not offset
    oled_sendCom(0xd5); //set display clock divide ratio
    oled_sendCom(0xf0); //set divide ratio
    oled_sendCom(0xd9); //set pre-charge period
    oled_sendCom(0x22); //
    oled_sendCom(0xda); //set com pins hardware configuration
    oled_sendCom(0x12);
    oled_sendCom(0xdb); //set vcomh
    oled_sendCom(0x20); //0.77xVcc
    oled_sendCom(0x8d); //set DC-DC enable
    oled_sendCom(0x14); //
    oled_sendCom(0xaf); //turn on oled panel

    oled_fill(0x00);
    oled_refresh();
}

void oled_display_off(void)
{
    oled_sendCom(0xae);
}

void oled_display_on(void)
{
    oled_sendCom(0xaf);
}

void oled_sendCom(uint8_t Com)
{
    iic_sendData(OLED_ADDR, 0x00, Com);
}

void oled_refresh(void)
{
    uint8_t buffer[129] = {0x40};
    for (uint8_t i = 0; i < 8; i++) {
        memcpy(buffer + 1, ROM[i], 128);
        oled_sendCom(0xb0 + i); //Set Page Start Address
        oled_sendCom(0x00); //set low column address
        oled_sendCom(0x10); //set high column address
        oled_sendData((uint8_t *)buffer, sizeof(buffer));
    }
}

void oled_sendData(uint8_t *Dat, uint16_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_ADDR & 0xFE, 1);
    i2c_master_write(cmd, Dat, size, 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1500 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);   
}

void oled_fill(uint8_t data)
{
    memset(ROM, data, 128 * 8);
}
