#include "common.h"

void parse_env(const char *filename) {
    FILE *file;
    char line[256];
    char key[64], value[64];
    
    write_log("PARSER", "FILE_PARSING", "Starting env.conf parsing");
    
    file = fopen(filename, "r");
    if (file == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to open %s", filename);
        write_log("PARSER", "FILE_PARSING", error_msg);
        perror("Error opening env.conf");
        exit(EXIT_FAILURE);
    }
    
    write_log("PARSER", "FILE_PARSING", "Successfully opened env.conf");
    
    // Initialize with default values
    strcpy(env_config.queue_name, "/emergenze");
    env_config.height = 0;
    env_config.width = 0;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        
        // Parse key=value format
        if (sscanf(line, "%63[^=]=%63s", key, value) != 2) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Invalid format in line: %s", line);
            write_log("PARSER", "FILE_PARSING", log_msg);
            continue;
        }
        
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Parsed: %s = %s", key, value);
        write_log("PARSER", "FILE_PARSING", log_msg);
        
        if (strcmp(key, "queue") == 0) {
            // Ensure queue name starts with '/'
            if (value[0] != '/') {
                snprintf(env_config.queue_name, sizeof(env_config.queue_name), "/%s", value);
            } else {
                strncpy(env_config.queue_name, value, sizeof(env_config.queue_name) - 1);
            }
            env_config.queue_name[sizeof(env_config.queue_name) - 1] = '\0';
            
            snprintf(log_msg, sizeof(log_msg), "Set queue name: %s", env_config.queue_name);
            write_log("PARSER", "FILE_PARSING", log_msg);
            
        } else if (strcmp(key, "height") == 0) {
            env_config.height = atoi(value);
            if (env_config.height <= 0) {
                write_log("PARSER", "FILE_PARSING", "Invalid height value, must be positive");
                exit(EXIT_FAILURE);
            }
            
            snprintf(log_msg, sizeof(log_msg), "Set height: %d", env_config.height);
            write_log("PARSER", "FILE_PARSING", log_msg);
            
        } else if (strcmp(key, "width") == 0) {
            env_config.width = atoi(value);
            if (env_config.width <= 0) {
                write_log("PARSER", "FILE_PARSING", "Invalid width value, must be positive");
                exit(EXIT_FAILURE);
            }
            
            snprintf(log_msg, sizeof(log_msg), "Set width: %d", env_config.width);
            write_log("PARSER", "FILE_PARSING", log_msg);
            
        } else {
            snprintf(log_msg, sizeof(log_msg), "Unknown configuration key: %s", key);
            write_log("PARSER", "FILE_PARSING", log_msg);
        }
    }
    
    fclose(file);
    
    // Validate final configuration
    if (env_config.height <= 0 || env_config.width <= 0) {
        write_log("PARSER", "FILE_PARSING", "Environment dimensions not properly configured");
        exit(EXIT_FAILURE);
    }
    
    char completion_msg[256];
    snprintf(completion_msg, sizeof(completion_msg), 
             "Environment parsing completed. Queue: %s, Dimensions: %dx%d", 
             env_config.queue_name, env_config.width, env_config.height);
    write_log("PARSER", "FILE_PARSING", completion_msg);
}