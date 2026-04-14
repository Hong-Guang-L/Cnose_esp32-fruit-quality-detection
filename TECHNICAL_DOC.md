# ESP32-S3 水果品质检测系统 — 技术文档

## 一、项目概述

本项目基于 **ESP32-S3** 开发板，使用 **5 路气体传感器** 采集水果挥发性气体数据，通过 **Edge Impulse** 训练的 **TensorFlow Lite** 神经网络模型进行实时推理分类，并将结果显示在 **OLED 屏幕** 上。

### 核心功能

| 功能 | 描述 |
|------|------|
| 数据采集 | 5 路 ADC 同步采集（H₂S / ODOR / HCHO / NH₃ / ETHANOL） |
| AI 推理 | Edge Impulse EON Compiler 编译的 TFLite 模型，本地端侧推理 |
| 结果输出 | 串口打印 + OLED 实时显示（分类标签 + 置信度） |

### 分类标签

模型输出 **3 个类别**：

| 标签 | 含义 |
|------|------|
| `air` | 无水果（背景空气） |
| `good` | 品质良好 |
| `bad` | 品质较差/变质 |

---

## 二、硬件架构

### 2.1 系统框图

```
┌─────────────────────────────────────────────────────────────┐
│                     ESP32-S3 开发板                          │
│                                                             │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐       │
│  │  H2S    │  │  ODOR   │  │  HCHO   │  │  NH3    │       │
│  │ GPIO7   │  │ GPIO1   │  │ GPIO2   │  │ GPIO5   │       │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘       │
│       │            │            │            │              │
│  ┌────┴────────────┴────────────┴────────────┴───┐        │
│  │           ADC_UNIT_1 (12-bit, 11dB)          │        │
│  └────────────────────┬─────────────────────────┘        │
│                       │                                   │
│  ┌────────────────────┴─────────────────────────┐        │
│  │         ETHANOL (GPIO6)                      │        │
│  └────────────────────┬─────────────────────────┘        │
│                       │                                   │
│  ┌────────────────────▼─────────────────────────┐        │
│  │     特征缓冲区 (15个float = 3帧 × 5传感器)    │        │
│  └────────────────────┬─────────────────────────┘        │
│                       │                                   │
│  ┌────────────────────▼─────────────────────────┐        │
│  │      TFLite 模型推理 (EON Compiler)          │        │
│  │      输入: 15维 → 输出: 3类概率               │        │
│  └────────────────────┬─────────────────────────┘        │
│                       │                                   │
│          ┌────────────┼────────────┐                      │
│          ▼            ▼            ▼                      │
│    ┌──────────┐ ┌─────────┐ ┌──────────────┐             │
│    │ UART串口 │ │ LED指示 │ │ SSD1306 OLED │             │
│    │ 115200   │ │ GPIO21  │ │ I2C (40/39)  │             │
│    └──────────┘ └─────────┘ └──────────────┘             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 引脚分配表

| 功能模块 | GPIO | ADC通道 | 说明 |
|----------|------|---------|------|
| **H₂S 传感器** | GPIO 7 | ADC1_CH6 | 硫化氢检测 |
| **ODOR 传感器** | GPIO 1 | ADC1_CH0 | 气味检测 |
| **HCHO 传感器** | GPIO 2 | ADC1_CH1 | 甲醛检测 |
| **NH₃ 传感器** | GPIO 5 | ADC1_CH4 | 氨气检测 |
| **ETHANOL 传感器** | GPIO 6 | ADC1_CH5 | 乙醇检测 |
| **状态 LED** | GPIO 21 | — | 推理时闪烁指示 |
| **OLED SDA** | GPIO 40 | — | I2C 数据线 |
| **OLED SCL** | GPIO 39 | — | I2C 时钟线 |

### 2.3 硬件参数

| 参数 | 值 |
|------|-----|
| MCU | ESP32-S3 (Xtensa® LX7, 双核 240MHz) |
| Flash | 2MB (SPI, 80MHz) |
| ADC 分辨率 | 12-bit (0~4095) |
| ADC 衰减 | 11dB (测量范围 0~3.3V) |
| 校准方式 | 曲线拟合校准 (Curve Fitting) |
| OLED 型号 | 0.96寸 SSD1306 (128×64 像素) |
| OLED I2C 地址 | 0x78 (7位地址) |
| I2C 时钟频率 | 400kHz |
| OLED 字体 | 12×24 ASCII (95字符) |

---

## 三、AI 模型详解

### 3.1 模型基本信息

| 属性 | 值 |
|------|-----|
| 平台 | Edge Impulse Studio v1.92.7 |
| 项目 ID | 951457 |
| 推理引擎 | TensorFlow Lite Micro (EON Compiler 编译) |
| 数据类型 | Float32 (未量化) |
| Arena 大小 | **3040 字节** (极小内存占用) |
| 分类阈值 | 0.6 (60%) |
| DSP 处理块数 | 1 (Raw Features) |
| 学习块数 | 1 (NN) |

### 3.2 数据流架构

```
原始采集 → [DSP: Raw Features] → [NN: TFLite] → [Softmax] → 分类结果
                                    ↓
                              15 维输入 → 隐藏层 → 3 类输出
```

#### 数据维度说明

```
EI_CLASSIFIER_RAW_SAMPLE_COUNT    = 3   (采样窗口内的采样点数)
EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME = 5  (每帧包含的传感器数量)
EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE  = 15  (总输入特征数 = 3 × 5)
EI_CLASSIFIER_NN_INPUT_FRAME_SIZE   = 15  (神经网络输入维度)
EI_CLASSIFIER_FREQUENCY             = 10  (采样频率 Hz)
EI_CLASSIFIER_INTERVAL_MS          = 100  (采样间隔 ms)
```

**关键理解**：每次推理需要 **3 个采样点**的数据（每点包含 5 个传感器的值），即一个 `float[15]` 的向量作为模型输入。

### 3.3 采样与推理时序

```
时间轴 (ms):  0     100   200   300   400   500 ...
              |      |     |     |     |     |
采样点:       [0]    [1]   [2]   [0]   [1]   [2] ...
              |←── 第1次推理 ──→|←── 第2次推理 ──→|

推理周期: 300ms (3个采样点 × 100ms间隔)
实际推理频率: ≈ 3.33 Hz
```

### 3.4 模型输入/输出

**输入张量**: `[1, 15]` — Float32

```
索引:  [0]   [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8]   [9]  [10]  [11]  [12]  [13]  [14]
含义:  h2s_0 odor_0 hcho_0 nh3_0 eth_0 h2s_1 odor_1 hcho_1 nh3_1 eth_1 h2s_2 odor_2 hcho_2 nh3_2 eth_2
       ─────── 第0次采样 ──────  ─────── 第1次采样 ──────  ─────── 第2次采样 ──────
```

**输出张量**: `[1, 3]` — Float32 (Softmax 归一化)

```
索引:  [0]    [1]    [2]
标签:  air    bad    good
值域:  0.0 ~ 1.0 (概率和为1.0)
```

### 3.5 Edge Impulse Impulse 配置

```c
// 来自 model_variables.h
const char* ei_classifier_inferencing_categories[] = { "air", "bad", "good" };
// 传感器融合类型: EI_CLASSIFIER_SENSOR_FUSION
// 融合字符串: "h2s + odor + hcho + nh3 + ethanol"
// DSP 配置: extract_raw_features (直接使用原始值, scale=1.0)
```

---

## 四、软件架构

### 4.1 文件结构

```
example-standalone-inferencing-espressif-esp32/
├── main/
│   ├── main.cpp              # 主程序：初始化、采样循环、推理、显示
│   ├── gas_sensors.c/h       # 5路气体传感器 ADC 驱动
│   ├── my_oled.c/h           # SSD1306 OLED 硬件 I2C 驱动
│   └── my_codetab.h          # 12×24 ASCII 字体数据
├── model-parameters/
│   ├── model_metadata.h      # 模型元数据宏定义
│   └── model_variables.h     # Impulse 完整配置（标签/DSP/NN）
├── tflite-model/
│   ├── tflite_learn_xxx_compiled.c/h  # EON 编译后的 TFLite 模型
│   └── trained_model_ops_define.h     # 模型算子定义
├── edge-impulse-sdk/         # Edge Impulse SDK (推理框架)
├── CMakeLists.txt            # 项目构建配置
├── sdkconfig                 # ESP-IDF 项目配置
└── README.md                 # 项目说明
```

### 4.2 核心代码流程

#### 主程序 (main.cpp) 流程图

```
app_main()
  │
  ├─→ setup_led()                    // 初始化 GPIO21 LED
  ├─→ nvs_flash_init()               // 初始化 NVS (非易失存储)
  │
  ├─→ oled_init(&oled_params)        // 初始化 OLED (I2C GPIO40/39)
  │     └─→ 显示 "Fruit Quality" + "Monitoring..."
  │
  ├─→ gas_sensors_init(&config)      // 初始化 5 路 ADC
  │     └─→ adc_oneshot_new_unit()   // 创建 ADC 单元
  │     └─→ 为每个通道配置校准
  │
  └─→ while(1) 主循环:
        │
        ├─→ gas_sensors_read()       // 读取 5 路传感器电压值(mV)
        │     └─→ adc_oneshot_read() // 单次 ADC 读取
        │     └─→ adc_cali_raw_to_voltage() // 校准转换为 mV
        │
        ├─→ 存入 features[] 缓冲区   // float[15], 循环覆盖
        │
        ├─→ feature_index == 0 ?     // 每 3 个采样点触发一次推理
        │     │
        │     ├─→ run_classifier()   // 执行 Edge Impulse 推理
        │     │     ├─→ DSP: extract_raw_features (提取15维特征)
        │     │     ├─→ NN: tflite_learn_invoke (TFLite 前向传播)
        │     │     └─→ Post: process_classification_f32 (Softmax)
        │     │
        │     ├─→ print_inference_result()  // 串口输出结果
        │     │
        │     └─→ 更新 OLED 显示          // 刷新分类结果+置信度
        │
        └─→ ei_sleep(100)            // 等待 100ms (10Hz 采样)
```

#### 关键数据结构

```c
// 特征缓冲区 - 滑动窗口
static float features[15] = {0};       // 3帧 × 5传感器
static int feature_index = 0;          // 当前写入位置 (0~2 循环)

// 传感器配置
static gas_sensors_config_t sensor_config = {
    .h2s_gpio = 7,
    .odor_gpio = 1,
    .hcho_gpio = 2,
    .nh3_gpio = 5,
    .ethanol_gpio = 6
};

// OLED 配置
oled_init_params_t oled_params = {
    .contrast = 0xFF,           // 最大亮度
    .lateral_flip = false,      // 不左右翻转
    .longitudinal_flip = false, // 不上下翻转
    .inverse = false            // 不反色
};
```

### 4.3 ADC 驱动细节 (gas_sensors.c)

**ADC 配置参数**:

| 参数 | 值 | 说明 |
|------|-----|------|
| ADC Unit | ADC_UNIT_1 | 使用 ADC1 (ESP32-S3 仅 ADC1 支持校准) |
| Attenuation | 11dB | 全量程衰减 (0~3.3V) |
| Bit Width | 12-bit | 0~4095 |
| Calibration | Curve Fitting | 两点曲线拟合校准 |

**GPIO → ADC 通道映射** (ESP32-S3):

```
GPIO1 → ADC1_CH0    GPIO5 → ADC1_CH4
GPIO2 → ADC1_CH1    GPIO6 → ADC1_CH5
GPIO7 → ADC1_CH6
```

**数据转换流程**:

```
物理电压 → ADC 采样 (raw: 0~4095) → 曲线拟合校准 → 电压值 (mV) → float 存入 features[]
```

### 4.4 OLED 驱动细节 (my_oled.c)

**驱动方案**: ESP-IDF 硬件 I2C (`driver/i2c.h`)

**为什么选择硬件 I2C**:
- 软件 I2C (GPIO bit-bang) 在 ESP32-S3 @160MHz 下时序不稳定
- 硬件 I2C 由 I2C 外设控制器管理，时序精确可靠
- 使用 `i2c_cmd_handle_t` API 构建命令链，批量发送效率高

**显存结构**:

```c
uint8_t ROM[8][128];  // 8页 × 128列 (对应 64行 × 128列像素)
                       // Page addressing mode
```

**刷新机制**:
1. 所有绘图操作写入 `ROM[][]` 显存（不立即发送）
2. 调用 `oled_refresh()` 将整个显存通过 I2C 一次性发送到屏幕
3. 减少总线占用，提高刷新效率

---

## 五、通信协议与输出格式

### 5.1 串口输出 (UART, 115200bps, 8N1)

**传感器数据** (每个采样周期):
```
h2s:1234,odor:2345,hcho:3456,nh3:4567,ethanol:5678
```

**推理结果** (每 300ms):
```
air:0.050 bad:0.120 good:0.830
result:good,confidence:0.830
```

### 5.2 OLED 显示布局

```
┌──────────────────────────────┐
│  Fruit Quality               │  ← 第0行, y=0
│  Result: good                │  ← 第2行, y=20
│  Conf: 83.0%                 │  ← 第4行, y=40
│                              │
│                              │
└──────────────────────────────┘
  <-------- 128px -------->
```

---

## 六、性能指标

| 指标 | 值 |
|------|-----|
| 采样频率 | 10 Hz (100ms/次) |
| 推理窗口 | 3 个采样点 (300ms) |
| 推理频率 | ≈ 3.33 Hz |
| 模型 Arena | 3040 bytes |
| 固件大小 | ~293 KB (0x48440 bytes) |
| Flash 占用 | 72% (1MB partition 中剩余 728KB) |
| RAM 占用 | 极低 (仅 3040B arena + 少量缓冲区) |

---

## 七、系统依赖

| 组件 | 版本 | 用途 |
|------|------|------|
| ESP-IDF | v5.3.1 | 开发框架 |
| Edge Impulse SDK | C++ Library | AI 推理框架 |
| TensorFlow Lite Micro | EON Compiled | 模型推理引擎 |
| ESP-DSP | 内置 | DSP 加速 (FFT 等) |
| ESP-NN | 内置 (S3) | 神经网络算子加速 |

### CMake 编译选项

```cmake
# 启用的优化
-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1      # ESP-NN 加速
-DEIDSP_USE_ESP_DSP=1                        # ESP-DSP 加速
-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3=1    # ESP32-S3 专用优化

# 编译标准
-std=gnu++17
```

---

## 八、更换/重新训练模型

当需要更新 AI 模型时：

1. 在 **Edge Impulse Studio** 中重新训练模型
2. 进入 **Deployment** 页面，选择 **"C++ library (Espressif ESP32)"**
3. 下载并解压，替换以下文件夹：
   ```
   ├── edge-impulse-sdk/
   ├── model-parameters/
   └── tflite-model/
   ```
4. 重新编译烧录：
   ```bash
   idf.py build flash monitor
   ```

> **注意**: 新模型的 `EI_CLASSIFIER_RAW_SAMPLE_COUNT` 等参数会自动更新，主程序无需修改（使用了宏引用）。
