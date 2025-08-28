CC = gcc
CFLAGS = -std=c11 -Wall -pedantic -g -pthread
LIBS = -lrt -lpthread

# Target executables
SERVER = server
CLIENT = client

# Common source files (parsers and utilities)
COMMON_SRCS = parse_rescuers.c parse_emergency_types.c parse_env.c utils.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

# Server specific files
SERVER_SRCS = server.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

# Client specific files
CLIENT_SRCS = client.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# All object files
ALL_OBJS = $(COMMON_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS)

.PHONY: default all clean run_server run_client setup_configs

default: $(SERVER) $(CLIENT)

all: $(SERVER) $(CLIENT)

# Generic rule for object files
%.o: %.c common.h
	$(CC) -c $(CFLAGS) $< -o $@

# Server executable
$(SERVER): $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Client executable  
$(CLIENT): $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Run server
run_server: $(SERVER)
	./$(SERVER)

# Run client (example usage)
run_client: $(CLIENT)
	./$(CLIENT) Incendio 100 150 2

# Run client with file
run_client_file: $(CLIENT)
	./$(CLIENT) -f emergencies.txt

# Setup example configuration files
setup_configs:
	@echo "Creating example configuration files..."
	@echo "[Pompieri][5][2][100;200]" > rescuers.conf
	@echo "[Ambulanza][3][4][150;250]" >> rescuers.conf
	@echo "[Polizia][4][3][50;100]" >> rescuers.conf
	@echo "" >> rescuers.conf
	@echo "[Incendio][2]Pompieri:2,8;Ambulanza:1,2;" > emergency_types.conf
	@echo "[Allagamento][1]Pompieri:1,5;Ambulanza:1,3;" >> emergency_types.conf
	@echo "[Sommossa][2]Polizia:3,6;Ambulanza:2,4;" >> emergency_types.conf
	@echo "[Incidente][1]Ambulanza:2,5;Polizia:1,3;" >> emergency_types.conf
	@echo "" >> emergency_types.conf
	@echo "queue=emergenze" > env.conf
	@echo "height=300" >> env.conf  
	@echo "width=400" >> env.conf
	@echo "" >> env.conf
	@echo "Incendio 120 180 1" > emergencies.txt
	@echo "Allagamento 50 75 3" >> emergencies.txt
	@echo "Sommossa 200 250 2" >> emergencies.txt
	@echo "Incidente 80 90 4" >> emergencies.txt
	@echo "Configuration files created successfully!"

# Test run with configurations
test: setup_configs $(SERVER) $(CLIENT)
	@echo "Starting test scenario..."
	@echo "1. Starting server in background..."
	./$(SERVER) &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "2. Sending test emergencies..."; \
	./$(CLIENT) -f emergencies.txt; \
	echo "3. Sending additional single emergency..."; \
	./$(CLIENT) Incendio 300 200 1; \
	sleep 10; \
	echo "4. Stopping server..."; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	echo "Test completed!"

# Clean build artifacts
clean:
	rm -f $(ALL_OBJS) $(SERVER) $(CLIENT)

# Clean everything including logs and configs
clean_all: clean
	rm -f *.log *.conf *.txt

# Show help
help:
	@echo "Emergency Management System Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  default      - Build server and client (same as 'make')"
	@echo "  all          - Build server and client"
	@echo "  server       - Build server only"
	@echo "  client       - Build client only"
	@echo "  setup_configs- Create example configuration files"
	@echo "  test         - Run complete test scenario"
	@echo "  run_server   - Run the server"
	@echo "  run_client   - Run client with example emergency"
	@echo "  run_client_file - Run client with file input"
	@echo "  clean        - Remove build artifacts"
	@echo "  clean_all    - Remove build artifacts, logs, and configs"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make setup_configs  # Create example config files"
	@echo "  make                # Build both executables"
	@echo "  make run_server     # Start server in one terminal"
	@echo "  make run_client     # Send emergency from another terminal"

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: $(SERVER) $(CLIENT)

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean $(SERVER) $(CLIENT)