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
	mkdir -p $(TARGET)
	mkdir -p /home/spark/request_spark/armcontrol/scripts
	sudo cp $(TARGET) /usr/bin/
	sudo chmod +x /usr/bin/armcontrol
	cp scripts/reset.py /home/spark/request_spark/armcontrol/scripts
	chmod +x /home/spark/request_spark/armcontrol/scripts/reset.py
	@echo "Installation completed!"

clean:
	@echo "Cleaning..."
	rm -f $(TARGET)
	@echo "Clean completed!"

