CC = g++
CFLAGS = -L/usr/lib -luarm -lserial -pthread -std=c++17
TARGET = out/armcontrol
SRCS = src/armcontrol.cpp

.PHONY: all install clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "Compiling..."
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS)
	@echo "Compilation completed!"

install: $(TARGET)
	@echo "Installing..."
	sudo cp $(TARGET) /usr/bin/
	@echo "Installation completed!"

clean:
	@echo "Cleaning..."
	rm -f $(TARGET)
	@echo "Clean completed!"

