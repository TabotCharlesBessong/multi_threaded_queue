CC = gcc
# CFLAGS for C11 standard, all warnings, pedantic checks, and debug symbols
CFLAGS = -std=c11 -Wall -pedantic -g
# Libraries needed for POSIX threads and real-time extensions (for message queues)
# Note: -lrt is not available on Windows, so we'll handle this conditionally
LIBS = -lpthread

# Executable names
SERVER_NAME = server
CLIENT_NAME = client

# Source files
# Common source files used by both server and client
COMMON_SRCS = parsers.c
# Server-specific source files
SERVER_SRCS = server.c server_impl.c $(COMMON_SRCS)
# Client-specific source files
CLIENT_SRCS = client.c

# Object files
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# Phony targets are not files. 'all' is the default target.
.PHONY: all clean run_server run_client

# Default target: build both executables
all: $(SERVER_NAME) $(CLIENT_NAME)

# Rule to link the server executable
$(SERVER_NAME): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Rule to link the client executable
$(CLIENT_NAME): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Generic rule to compile .c files into .o object files
%.o: %.c common.h macros.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to run the server
run_server: $(SERVER_NAME)
	./$(SERVER_NAME)

# Rule to run the client (example usage)
run_client: $(CLIENT_NAME)
	./$(CLIENT_NAME) Incendio 10 20 0

# Rule to clean up object files and executables
clean:
	rm -f $(SERVER_NAME) $(CLIENT_NAME) $(SERVER_OBJS) $(CLIENT_OBJS)