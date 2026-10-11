CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -pthread
SRC = src/main.c src/proc_stats.c src/process_monitor.c src/json_writer.c src/dashboard.c src/python_bridge.c
OUT = ai_monitorx

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)
	rm -f /tmp/ai_monitorx_stats.json /tmp/ai_monitorx_stats.json.tmp

.PHONY: all clean
