# 專案開發藍圖：基於 FreeRTOS 的非同步感測資料擷取與日誌系統 (Data Logger)

## 📌 專案概述
* **目標**：實作一個獨立運作的資料記錄器 (Data Logger)，模擬工業級嵌入式系統在無預警斷電或當機時的保存機制，可以透過 UART console 撈 log 追查。
* **開發週期**：4 週 (3 週實作 + 1 週緩衝與優化)
* **核心技術棧**：C 語言, STM32 Cortex-M4, FreeRTOS, I2C (Bit-banging), SPI, UART DMA
* **硬體配置**：
  * **MCU**: STM32 Nucleo-F446RE (ARM Cortex-M4 帶 FPU)
  * **Sensor**: LM75 (I2C 數位溫度感測器)
  * **Storage**: W25Q64 (SPI Flash, 64Mbit)
  * **Tool**: 24M 8CH 邏輯分析儀 (搭配 PulseView)

---

## 🏗️ 系統規格設計

1. **`背景巡視`
   * 定期讀取 LM75 溫度，浮點數轉整數成本。
2. **`異常警報`
   * 溫度超過連續幾次的閥值會觸發警報 --> 連續是為了避免雜訊誤觸 
3. **`時間來源`
   * (A) 內部 RTC + VBAT 鈕扣電池，斷電持續計時
   * (B) boot counter + 開機後 ticks
4. **`紀錄格式`
   * 搭配 Lock-free Ring Buffer 接收 CLI 輸入字元，保護系統效能。
   * 固定 16 bytes，因為 W25Q64 page size = 256 bytes，page 不能跨頁。
5. **`儲存方式`
   * SPI 寫入 W25Q64 (8MB)，Flash ring buffer，寫滿抹除最舊 sector。
6. **`斷電復原`
7. **`CLI`
   * UART 中斷接收，non-blocking 解析

---

## 🏗️ 系統架構設計

系統劃分為 3 個核心 Task 與 1 個中斷 ISR，以達成非同步的高穩定運作：

1. **`Sensor_Task` (資料擷取任務 - 中優先級)**
   * 使用 `vTaskDelayUntil` 確保絕對執行週期，避免 Drift。
   * 定期讀取 LM75 溫度，打上 Timestamp (Tick Count)，將 `Data_Packet` 送入 Queue。
2. **`Storage_Task` (持久化儲存任務 - 低優先級)**
   * 阻塞等待 Queue 資料。
   * 將接收到的資料透過 SPI 寫入 W25Q64 Flash，負責磨損平衡與位址管理。
3. **`CLI_Task` (命令列互動任務 - 最低優先級)**
   * 透過 UART 解析工程師指令 (如 `help`, `dump_log`, `clear_log`)。
   * 使用純軟體 `strcmp` 實作指令辨識與選單。
4. **`UART_DMA_ISR` (背景通訊中斷)**
   * 搭配 Lock-free Ring Buffer 接收 CLI 輸入字元，保護系統效能。

---

## 📅 四週計畫

### Week 1：C 語言核心與純軟體通訊底層 (無硬體先行)
* **指標與記憶體基底**：深度複習指標 (Pointer) 操作、記憶體對齊 (Memory Alignment) 觀念，並確立 `volatile` 的正確使用時機（防止編譯器將中斷會更改的變數優化掉）。
* **Lock-free Ring Buffer 實作**：
  * 實作單生產者單消費者 (SPSC) 模型。
  * **嚴格無鎖條件成立**：明確規範 ISR 僅能修改 `head`，Task 僅能修改 `tail`。
  * **效能與防護**：利用 N-1 滿水位機制與位元遮罩 (`& 0xFF`) 優化運算效能，並建立防護邏輯以杜絕 Buffer Overflow。
* **純軟體 CLI 解析器 (PC 端模擬 UART)**：
  * 實作字串接收與斷句邏輯。
  * 運用 Thread-safe 的 `strtok_r` 與 `strcmp` 進行指令切割與辨識，建立具備擴充性的 `help` 選單架構。
* **HAL Timebase 轉移 (RTOS)**：將 STM32 HAL 的系統時基從預設的 `SysTick` 移至硬體計時器 (如 TIM6)，保留 `SysTick` 給下一階段的 FreeRTOS Scheduler 專用。

### Week 2：通訊協議實作與硬體驅動 (Bare-metal)
* **I2C Bit-banging (LM75 溫度感測器)**：
  * 不依賴硬體 I2C，手刻 GPIO Open-drain + Pull-up 時序，以利 Debug 並加深對底層協議的理解。
  * 實作 **9-Clock Bus Recovery** 機制，當系統重置導致 SDA 線被 Slave 咬死時，主動打 Clock 解鎖匯流排。
* **SPI Flash 暫存器級操作 (W25Q64)**：
  * 學習 SPI 硬體狀態機。驗證 JEDEC ID (`0xEF 40 17`)。
  * 實作 Write Enable (`0x06`)、Sector Erase (`0x20`) 與 Page Program (`0x02`)。
  * 實作 Polling 狀態暫存器的 WIP (Write In Progress) bit，確保寫入完成。
* **Flash 持久化位址管理**：實作輕量級 Flash Ring Buffer，系統開機時自動掃描尋找最後一次的寫入位址，確保新日誌能無縫銜接。

### Week 3：FreeRTOS 移植與任務多工架構
* **任務解耦與建立**：導入 FreeRTOS，將系統拆分為獨立的 `Sensor_Task`、`Storage_Task` 與 `CLI_Task`。使用 `vTaskDelayUntil` 確保 Sensor 任務的絕對執行週期，避免 Drift。
* **中斷與優先級陷阱迴避**：
  * 設定 `NVIC_PRIORITYGROUP_4`。
  * 釐清 Cortex-M (數字越小越優先) 與 FreeRTOS (數字越大越優先) 優先級邏輯相反的地雷。
  * 確保 ISR 呼叫 FromISR API 時，其優先權限設定正確 (`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`)。
* **任務通訊與上下文切換 (Context Switch)**：
  * 運用 Queue 在任務間傳遞結構體 (`Data_Packet`)，並用 Mutex 保護 SPI Bus 避免資料交錯。
  * 在 UART 接收 ISR 結尾實作 `portYIELD_FROM_ISR()`，強制 Context Switch 即時喚醒 CLI 任務。

### Week 4：系統整合、防呆與壓力測試
* **硬體 UART DMA 整合**：將 Week 1 純軟體開發的 CLI 引擎正式掛載至 STM32 的 UART DMA 背景接收與發送。
* **高可靠度機制 (Watchdog)**：實作獨立看門狗 (IWDG)，在最低優先級 Task 定期餵狗。設計故意觸發的死迴圈 Bug，驗證系統能否自動重啟並保留當機前的日誌。
* **面試技術盤點**：使用 PulseView 邏輯分析儀截圖 SPI/I2C 實際時序。整理 Priority Inversion (優先權反轉)、無鎖條件等韌體面試必考精華。

### 專案資料夾
  📂 你的專案根目錄 (My_DataLogger)
 ┣ 📂 Core            <-- (底層配置：如果是用工具生成的代碼都放這)
 ┃  ┣ 📂 Inc        (例如 main.h, stm32f4xx_it.h)
 ┃  ┗ 📂 Src        (例如 main.c, stm32f4xx_it.c)
 ┃
 ┣ 📂 App             <-- (你的應用邏輯：面試最核心的價值放這)
 ┃  ┣ 📂 Inc        (ring_buffer.h, cli_task.h, data_logger.h)
 ┃  ┗ 📂 Src        (ring_buffer.c, cli_task.c, data_logger.c)
 ┃
 ┣ 📂 Drivers         <-- (硬體驅動層：你自己寫的外部 IC 驅動)
 ┃  ┣ 📂 Inc        (lm75.h, w25q64.h, bme280_if.h)
 ┃  ┗ 📂 Src        (lm75.c, w25q64.c, bme280_if.c)
 ┃
 ┗ 📂 Middlewares     <-- (第三方函式庫：別人的 Code)
    ┗ 📂 FreeRTOS   (FreeRTOS 的原始碼)