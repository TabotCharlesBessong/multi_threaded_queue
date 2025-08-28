#include "common.h"

static mqd_t message_queue = -1;
static volatile sig_atomic_t terminate_flag = 0;

void cleanup_client(void) {
    if (message_queue != -1) {
        mq_close(message_queue);
        write_log("CLIENT", "MESSAGE_QUEUE", "Message queue closed");
    }
    cleanup_logging();
}

void sigint_handler(int sig) {
    terminate_flag = 1;
    const char msg[] = "\nClient termination requested...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

void send_emergency_request(const char *emergency_name, int x, int y, int delay) {
    char log_msg[256];
    
    // Validate coordinates
    if (x < 0 || x >= env_config.width || y < 0 || y >= env_config.height) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Invalid coordinates (%d,%d) for emergency %s", x, y, emergency_name);
        write_log("CLIENT", "VALIDATION", log_msg);
        printf("Error: Invalid coordinates (%d,%d)\n", x, y);
        return;
    }
    
    // Validate emergency type exists
    if (find_emergency_type(emergency_name) == NULL) {
        snprintf(log_msg, sizeof(log_msg), "Unknown emergency type: %s", emergency_name);
        write_log("CLIENT", "VALIDATION", log_msg);
        printf("Error: Unknown emergency type '%s'\n", emergency_name);
        return;
    }
    
    // Wait for delay
    if (delay > 0) {
        snprintf(log_msg, sizeof(log_msg), "Waiting %d seconds before sending emergency", delay);
        write_log("CLIENT", "TIMING", log_msg);
        printf("Waiting %d seconds before sending emergency...\n", delay);
        
        for (int i = 0; i < delay && !terminate_flag; i++) {
            sleep(1);
        }
        
        if (terminate_flag) {
            write_log("CLIENT", "TERMINATION", "Emergency sending cancelled due to termination request");
            return;
        }
    }
    
    // Create emergency request
    emergency_request_t request;
    strncpy(request.emergency_name, emergency_name, EMERGENCY_NAME_LENGTH - 1);
    request.emergency_name[EMERGENCY_NAME_LENGTH - 1] = '\0';
    request.x = x;
    request.y = y;
    request.timestamp = time(NULL);
    
    // Send message
    if (mq_send(message_queue, (const char*)&request, sizeof(request), 0) == -1) {
        snprintf(log_msg, sizeof(log_msg), "Failed to send emergency %s", emergency_name);
        write_log("CLIENT", "MESSAGE_QUEUE", log_msg);
        perror("Failed to send emergency request");
        return;
    }
    
    snprintf(log_msg, sizeof(log_msg), 
             "Sent emergency: %s at (%d,%d) timestamp=%ld", 
             emergency_name, x, y, (long)request.timestamp);
    write_log("CLIENT", "MESSAGE_QUEUE", log_msg);
    printf("Emergency sent: %s at coordinates (%d,%d)\n", emergency_name, x, y);
}

void process_file(const char *filename) {
    FILE *file;
    char line[256];
    char emergency_name[64];
    int x, y, delay;
    int line_number = 0;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Processing file: %s", filename);
    write_log("CLIENT", "FILE_PROCESSING", log_msg);
    
    file = fopen(filename, "r");
    if (file == NULL) {
        snprintf(log_msg, sizeof(log_msg), "Failed to open file: %s", filename);
        write_log("CLIENT", "FILE_PROCESSING", log_msg);
        perror("Failed to open input file");
        return;
    }
    
    while (fgets(line, sizeof(line), file) != NULL && !terminate_flag) {
        line_number++;
        
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        // Parse line
        if (sscanf(line, "%63s %d %d %d", emergency_name, &x, &y, &delay) != 4) {
            snprintf(log_msg, sizeof(log_msg), 
                     "Invalid format in file %s line %d: %s", filename, line_number, line);
            write_log("CLIENT", "FILE_PROCESSING", log_msg);
            printf("Warning: Invalid format in line %d\n", line_number);
            continue;
        }
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Read from file line %d: %s %d %d %d", 
                 line_number, emergency_name, x, y, delay);
        write_log("CLIENT", "FILE_PROCESSING", log_msg);
        
        send_emergency_request(emergency_name, x, y, delay);
    }
    
    fclose(file);
    
    snprintf(log_msg, sizeof(log_msg), "File processing completed: %s", filename);
    write_log("CLIENT", "FILE_PROCESSING", log_msg);
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    
    // Initialize logging
    init_logging();
    write_log("CLIENT", "STARTUP", "Client application started");
    
    // Parse configuration files
    parse_env("env.conf");
    parse_rescuers("rescuers.conf");
    parse_emergency_types("emergency_types.conf");
    
    // Setup signal handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        write_log("CLIENT", "SIGNAL", "Failed to setup SIGINT handler");
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    
    // Open message queue
    message_queue = mq_open(env_config.queue_name, O_WRONLY);
    if (message_queue == -1) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to open message queue: %s", env_config.queue_name);
        write_log("CLIENT", "MESSAGE_QUEUE", log_msg);
        perror("Failed to open message queue");
        printf("Make sure the server is running first.\n");
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Connected to message queue: %s", env_config.queue_name);
    write_log("CLIENT", "MESSAGE_QUEUE", log_msg);
    
    // Process command line arguments
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s <emergency_name> <x> <y> <delay>\n", argv[0]);
        printf("  %s -f <filename>\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s Incendio 100 150 5\n", argv[0]);
        printf("  %s -f emergencies.txt\n", argv[0]);
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    
    if (strcmp(argv[1], "-f") == 0) {
        // File mode
        if (argc != 3) {
            printf("Usage: %s -f <filename>\n", argv[0]);
            cleanup_client();
            exit(EXIT_FAILURE);
        }
        
        write_log("CLIENT", "MODE", "Running in file mode");
        process_file(argv[2]);
        
    } else {
        // Command line mode
        if (argc != 5) {
            printf("Usage: %s <emergency_name> <x> <y> <delay>\n", argv[0]);
            cleanup_client();
            exit(EXIT_FAILURE);
        }
        
        write_log("CLIENT", "MODE", "Running in command line mode");
        
        char *emergency_name = argv[1];
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        int delay = atoi(argv[4]);
        
        if (delay < 0) {
            printf("Error: Delay cannot be negative\n");
            cleanup_client();
            exit(EXIT_FAILURE);
        }
        
        send_emergency_request(emergency_name, x, y, delay);
    }
    
    write_log("CLIENT", "SHUTDOWN", "Client application completed");
    cleanup_client();
    return 0;
}