# 定義編譯器
CC = gcc

# 定義編譯參數 (CFLAGS)
CFLAGS = -std=c11 -Wall -Wextra -Wshadow -Wconversion -Wpedantic -O2 -Iapp/inc

# 定義要編譯的原始碼檔案
TEST_SRCS = test/test.c app/src/ring_buffer.c app/src/cli_task.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGET = test_runner

all: test

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# 連結所有 .o 檔生成執行檔
$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# 編譯每個 .c 變成 .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清除編譯產生的暫存檔
clean:
	rm -f $(TEST_OBJS) $(TEST_TARGET)