# 專案：基於 FreeRTOS 的車內高溫安全監控與日誌系統

## 📌 專案概述
* **目標**：實作一個獨立運作的車內環境監控黑盒子。結合即時溫度警報、OLED 狀態顯示、無線遠端遙測，並具備無預警斷電時的日誌保存 (Last Gasp) 機制，可透過 CLI 撈取歷史紀錄。
* **開發週期**：4 週 (3 週實作 + 1 週緩衝與優化)
* **核心技術棧**：C 語言, STM32 Cortex-M4, FreeRTOS (Queue, Mutex, Event Group, Software Timer), I2C, SPI, UART DMA
* **硬體配置**：
  * **MCU**: STM32 Nucleo-F446RE (ARM Cortex-M4 帶 FPU)
  * **Sensor**: LM75 (I2C 數位溫度感測器)
  * **Storage**: W25Q64 (SPI Flash, 64Mbit)
  * **Display**: 0.96" OLED (I2C 介面, SSD1306 驅動)
  * **Wireless**: ESP8266 (Wi-Fi) 或 HC-05 (藍牙)
  * **Alert**: 開發板內建 LED (PA5)
  * **Tool**: 24M 8CH 邏輯分析儀 (搭配 PulseView)

---

## ⚙️ 系統規格設計

1. **背景巡視 (Polling)**：定期讀取 LM75 溫度，考量硬體 FPU 效能，評估浮點數轉整數的運算成本。
2. **多重異常狀態機 (Complex Alert)**：警報不再僅限於單一溫度觸發，而是綜合「高溫危險」、「儲存體寫滿/失效」、「通訊異常」等多重事件進行狀態判定。
3. **時間來源 (Timestamp)**：採用 Boot Counter + 開機後 Ticks 進行相對時間記錄（若後續擴充 RTC + VBAT 則切換為絕對時間）。
4. **紀錄格式 (Data Payload)**：固定 16 Bytes 結構體。考量 W25Q64 Page Size = 256 Bytes，確保每 16 筆資料完美對齊不跨頁。
5. **儲存方式 (Wear Leveling)**：SPI 寫入 W25Q64 (8MB)，實作 Flash Ring Buffer，寫滿時自動抹除最舊的 Sector。
6. **斷電復原 (Recovery)**：開機時自動掃描 Flash，定位最後寫入位址以無縫銜接。
7. **無鎖通訊 (CLI / Wireless)**：透過 UART 中斷與 SPSC Ring Buffer 接收指令，實作 Non-blocking 的指令解析器。

---

## 🏗️ FreeRTOS 系統架構設計 (IPC 資源分配)

系統劃分為 4 個核心 Task、1 個事件守護者、1 個軟體定時器與 1 個中斷 ISR，徹底展現 RTOS 的多工排程與資源保護能力：

1. **`Sensor_Task` (資料擷取任務 - 中優先級)**
   * 使用 `vTaskDelayUntil` 確保絕對執行週期，避免計時飄移 (Drift)。
   * 負責 I2C 讀取 LM75。若溫度連續超標，設置 **Event Group** 的 `TEMP_WARNING_BIT`。
   * 將打包好的感測資料透過 **Queue** 送給 Storage_Task。
2. **`Display_Task` (面板刷新任務 - 低優先級)**
   * 負責更新 OLED 畫面 (溫度、系統狀態)。
   * 與 Sensor_Task 共用 I2C 匯流排，需使用 **Mutex** 進行互斥保護，並展示優先權繼承 (Priority Inheritance) 機制防範優先權反轉。
3. **`Storage_Task` (持久化儲存任務 - 低優先級)**
   * 阻塞等待 Queue 內的感測資料，取得後透過 SPI 寫入 W25Q64。
   * 若偵測到 Flash 容量已滿或 SPI 寫入失敗，設置 **Event Group** 的 `FLASH_ERROR_BIT`。
4. **`Wireless_CLI_Task` (無線與命令列任務 - 低優先級)**
   * 處理來自電腦或無線模組的指令字串。使用 `strcmp` 解析指令，執行查閱動作。
   * 若發生 UART 斷線或緩衝區溢位，設置 **Event Group** 的 `UART_ERROR_BIT`。
5. **`Alert_Manager_Task` (警報狀態機任務 - 最高優先級)**
   * 平時使用 `xEventGroupWaitBits()` 處於 Blocked 狀態不佔 CPU。
   * 綜合等待所有任務異常狀態 (支援 AND/OR 邏輯)。根據觸發的 Bit 組合，動態決定警報層級，並透過 FreeRTOS API 變更軟體定時器的週期。
6. **`LED_Blink_Timer` (非阻塞硬體控制 - Software Timer)**
   * 使用 FreeRTOS **Auto-reload Software Timer** 來實作 LED 閃爍。
   * 根據警報層級由 `Alert_Manager_Task` 動態調整閃爍頻率（如：警告 1Hz，危險 5Hz），完全不使用 Task 內的迴圈 Delay，展現系統背景定時器的高階應用。
7. **`UART_DMA_ISR` (背景通訊中斷)**
   * 將收到的字元 Push 進入 Lock-free Ring Buffer，並利用 `portYIELD_FROM_ISR()` 喚醒 CLI_Task。

---

## 📅 四週衝刺計畫

### Week 1：C 語言核心與純軟體通訊底層 (無硬體先行)
* **指標與記憶體基底**：複習指標操作、Memory Alignment，確保 `volatile` 正確防護 ISR 變數。
* **Lock-free Ring Buffer**：實作 SPSC 模型，確立 ISR 寫 head、Task 寫 tail 的無鎖條件。利用 N-1 機制與位元遮罩 (`& 0xFF`) 達成防護與極致效能。
* **純軟體 CLI 解析器**：運用 Thread-safe 的 `strtok_r` 與 `strcmp`，建立具備擴充性的 Non-blocking 系統選單。
* **HAL Timebase 轉移**：將 STM32 HAL 時基從 `SysTick` 移至 TIM6，為 FreeRTOS Scheduler 鋪路。

### Week 2：通訊協議實作與硬體驅動 (Bare-metal)
* **I2C 匯流排通訊 (LM75 & OLED)**：
  * 手刻 GPIO Open-drain + Pull-up 時序，實作 I2C 底層。
  * 實作 **9-Clock Bus Recovery** 機制，解鎖被掛死的 SDA 線。
  * 移植 SSD1306 OLED 顯示驅動。
* **SPI Flash 暫存器級操作 (W25Q64)**：
  * 驗證 JEDEC ID，實作 Write Enable、Sector Erase、Page Program 與 Polling WIP bit 狀態機。
  * 完成 Flash 啟動位址尋找與環形覆寫邏輯。
* **UART 外部通訊**：打通與通訊模組的 AT Command / 資料收發底層。

### Week 3：FreeRTOS 移植與任務 IPC 實戰
* **任務解耦與建立**：掛載 FreeRTOS，拆分 Sensor, Storage, CLI, Display, Alert_Manager 等任務。
* **中斷優先級陷阱迴避**：設定 `NVIC_PRIORITYGROUP_4`，釐清 Cortex-M 與 RTOS 優先級數字邏輯。
* **任務間通訊 (IPC) 深度整合**：
  * 實作 **Queue** 傳遞感測資料。
  * 實作 **Mutex** 保護 I2C 避免 OLED 與 LM75 封包交錯。
  * 實作 **Event Group** 處理多重任務的異常通報，取代單一的 Task Notification。
  * 啟動 **Software Timer Daemon**，完成非阻塞式的 LED 頻率控制。

### Week 4：系統整合、防呆與進階電源管理
* **進階電源管理 (Tickless Idle)**：實作熄火省電模式，啟動 FreeRTOS 深度睡眠機制，降低閒置功耗。
* **高可靠度機制 (Watchdog)**：實作獨立看門狗 (IWDG)，驗證死迴圈狀態下系統的自動重啟與日誌銜接。
* **面試技術盤點**：擷取 PulseView SPI/I2C 波形。統整 Mutex 優先權繼承、無鎖 Buffer 設計、Context Switch 流程等關鍵問答。