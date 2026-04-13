# ESP32-S3 水果品质检测

基于 Edge Impulse AI 模型，使用 5 路气体传感器进行水果品质分类，OLED 实时显示结果。

## 引脚分配

| 功能 | GPIO | 说明 |
|------|------|------|
| **气体传感器 H2S** | GPIO7 | ADC 采集 |
| **气体传感器 ODOR** | GPIO1 | ADC 采集 |
| **气体传感器 HCHO** | GPIO2 | ADC 采集 |
| **气体传感器 NH3** | GPIO5 | ADC 采集 |
| **气体传感器 ETHANOL** | GPIO6 | ADC 采集 |
| **状态指示灯** | GPIO21 | 推理时闪烁 |
| **OLED SDA** | GPIO40 | I2C 数据线 |
| **OLED SCL** | GPIO39 | I2C 时钟线 |

## 硬件要求

- 开发板：ESP32-S3
- OLED 屏幕：0.96 寸 SSD1306（128x64，I2C 地址 0x78）
- 气体传感器 x5（H2S / ODOR / HCHO / NH3 / ETHANOL）

## 编译与烧录

### 1. 环境准备

```bash
# 激活 ESP-IDF（已安装 v5.3.1）
get_idf
```

### 2. 编译

```bash
cd example-standalone-inferencing-espressif-esp32
idf.py build
```

### 3. 烧录

```bash
# Linux/Mac
idf.py -p /dev/ttyUSB0 flash monitor

# Windows
idf.py -p COM3 flash monitor
```

串口波特率：115200

## 输出说明

- **串口输出**：`result:标签,confidence:概率`
- **OLED 显示**：分类结果 + 置信度百分比

## 更换模型

从 Edge Impulse 导出 C++ 库后，替换以下文件夹：

```
├── edge-impulse-sdk/
├── model-parameters/
├── tflite-model/
```

然后重新编译烧录即可。
