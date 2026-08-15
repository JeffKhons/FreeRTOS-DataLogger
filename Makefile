# ==========================================
# PC 端測試專用 Makefile
# ==========================================

# 定義編譯器
CC = gcc

# 定義編譯參數 (CFLAGS)
# -Wall: 開啟所有警告
# -g: 加入除錯資訊 (方便未來用 GDB)
# -I: 告訴編譯器要去哪裡找 .h 檔
CFLAGS = -Wall -g -Icore/inc -Iapp/inc

# 定義要編譯的原始碼檔案
SRCS = test.c \
       app/src/ring_buffer.c \
       app/src/cli_task.c

# 將 .c 替換成 .o (目的檔)
OBJS = $(SRCS:.c=.o)

# 最終輸出的執行檔名稱
TARGET = test.exe

# ==========================================
# 編譯規則
# ==========================================

# 預設目標：打 make 就會執行這個
all: $(TARGET)

# 連結所有 .o 檔生成執行檔
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build successful! Run ./$(TARGET) to test."

# 編譯每個 .c 變成 .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清除編譯產生的暫存檔
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned up."