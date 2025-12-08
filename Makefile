CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11 -pthread

SRC = src/main.c src/dsp.c src/fir.c src/worker.c
OBJ = $(SRC:.c=.o)
DEPS = src/dsp.h src/fir.h src/worker.h src/dsp.h

TARGET = dsp_engine

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) $(TARGET)
