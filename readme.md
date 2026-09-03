# STM32F446RE FreeRTOS 溫度監控與 Wi-Fi 回報系統

## 專案目標

本專案以 STM32 Nucleo-F446RE 為核心，採用手刻 register-level driver 與 FreeRTOS，建立可即時量測、顯示、告警與無線回報的溫度監控系統。

不使用 STM32CubeMX 自動產生的 HAL 周邊初始化；CMSIS 僅用於 STM32 暫存器定義與 Cortex-M4 core API。

## 目前硬體

- MCU：STM32F446RE（Cortex-M4F）
- Sensor：LM75，使用 I2C1
- Display：0.96 inch SSD1306 OLED，與 LM75 共用 I2C1
- CLI：PuTTY 經 USART2 DMA RX + IDLE interrupt
- Wireless：ESP8266，尚未實作
- Alert：Nucleo 板載 LED（規劃使用 PA5）
- Storage：W25Q64 SPI Flash 驅動程式保留為實驗紀錄，目前不納入專案 (硬體問題)

## 已完成項目

### Bare-metal driver

- HSI PLL 系統時鐘：84 MHz
- I2C1：PB8/SCL、PB9/SDA，100 kHz timing 與 DWT 9-clock bus recovery
- LM75 address scan：掃描 0x48 到 0x4F，以因應 A0-A2 硬體位址無法固定的狀況
- SSD1306 OLED 顯示
- USART2：115200 baud、DMA1 Stream5 circular RX、IDLE line interrupt
- Lock-free SPSC ring buffer：ISR 寫入、CLI consumer 讀取

### FreeRTOS 基礎

- `CLI_Task`：由 USART2 ISR 的 `vTaskNotifyGiveFromISR()` 喚醒
- `xI2CMutex`：保護日後 LM75 與 OLED 對 I2C1 的共用存取
- dynamic allocation：`heap_4.c`，FreeRTOS heap 為 15 KB
- scheduler、SysTick、PendSV、SVC 已可正常啟動

## 目前 CLI

```text
help              顯示指令
read              直接讀取並印出 LM75 溫度
stat              顯示 UART ring buffer 與 logger 狀態
dump [n]          Flash 功能停用時會回報 no record
```

## SPI Flash 狀態

`bsp_spi.c`、`bsp_w25q64.c` 與相關測試程式會保留，作為 SPI、JEDEC ID、sector erase、page program、read-back verify 的學習紀錄。

目前 W25Q64 模組排針接觸不穩，且錯誤讀值曾造成 boot scan 將 Flash 誤判為已有大量記錄。因此 active firmware 不初始化 SPI1、不讀取 JEDEC ID、不掃描 Flash，也不會建立 Storage Task。日後若硬體修復，再以新的記錄格式重新整合。

## 下一階段架構

```text
LM75
  ↓ I2C mutex
Sensor_Task
  ├─→ Display Queue（只保留最新溫度）→ Display_Task → OLED
  ├─→ Telemetry Queue（只保留最新溫度）→ ESP8266_Task → Wi-Fi
  └─→ Event Group（溫度過高 / Sensor error）→ Alert_Manager_Task
                                                     ↓
                                            LED Software Timer → PA5 LED

USART2 DMA + IDLE ISR
  ↓ Task Notification
CLI_Task
```

## FreeRTOS 實作 roadmap

### Phase 1：Sensor + Display

1. 建立 `Sensor_Task`，以 `vTaskDelayUntil()` 固定週期讀取 LM75。
2. `Sensor_Task` 取得 `xI2CMutex` 後讀取溫度，再用 length-1 queue 將最新值交給 `Display_Task`。
3. `Display_Task` 取得同一把 `xI2CMutex`，更新 OLED。
4. CLI `read` 也必須以 `xI2CMutex` 保護 LM75 transaction。

### Phase 2：告警機制

1. Sensor Task 根據可設定的高溫閾值設定 Event Group bit。
2. `Alert_Manager_Task` 使用 `xEventGroupWaitBits()` block 等待警報。
3. Software Timer 控制 PA5 板載 LED：一般告警慢閃、嚴重告警快閃。

### Phase 3：ESP8266

1. 保留 USART2 給 PuTTY CLI；ESP8266 使用另一組 UART，避免 AT command 與 CLI 混線。
2. 建立 ESP8266 RX DMA / ring buffer / parser。
3. `ESP8266_Task` 完成 AT、ATE0、Wi-Fi join 與連線狀態機。
4. 將 Telemetry Queue 的最新溫度以 HTTP、MQTT 或 TCP 回報。

## 建置與燒錄

```text
make
```

輸出檔位於 `build/STM32_1.elf`、`build/STM32_1.hex`、`build/STM32_1.bin`。VS Code 的 Cortex-Debug 設定以 OpenOCD/ST-Link 啟動 F5 debug。
