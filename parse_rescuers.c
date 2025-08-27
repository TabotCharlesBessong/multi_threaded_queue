#include "common.h"

void parse_rescuers(const char *filename) {
    FILE *file;
    char line[256];
    char name[64];
    int num, speed, x, y;
    
    write_log("PARSER", "FILE_PARSING", "Starting rescuers.conf parsing");
    
    file = fopen(filename, "r");
    if (file == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to open %s", filename);
        write_log("PARSER", "FILE_PARSING", error_msg);
        perror("Error opening rescuers.conf");
        exit(EXIT_FAILURE);
    }
    
    write_log("PARSER", "FILE_PARSING", "Successfully opened rescuers.conf");
    
    rescuer_types_count = 0;
    rescuers_count = 0;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        // Parse line: [Name][Num][Speed][X;Y]
        if (sscanf(line, "[%63[^]]][%d][%d][%d;%d]", name, &num, &speed, &x, &y) != 5) {
            char log_msg[512]; // Increased buffer size
            // Truncate line for logging if too long
            char truncated_line[100];
            strncpy(truncated_line, line, sizeof(truncated_line) - 1);
            truncated_line[sizeof(truncated_line) - 1] = '\0';
            
            snprintf(log_msg, sizeof(log_msg), "Invalid format in line: %.90s", truncated_line);
            write_log("PARSER", "FILE_PARSING", log_msg);
            continue;
        }
        // Validate parsed data
        if (num <= 0 || speed <= 0) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Invalid values for rescuer %s: num=%d, speed=%d", name, num, speed);
            write_log("PARSER", "FILE_PARSING", log_msg);
            continue;
        }
        
        // Check if we have space for more rescuer types
        if (rescuer_types_count >= MAX_RESCUER_TYPES) {
            write_log("PARSER", "FILE_PARSING", "Maximum rescuer types exceeded");
            break;
        }
        
        // Create rescuer type
        rescuer_type_t *type = &rescuer_types[rescuer_types_count];
        type->rescuer_type_name = malloc(strlen(name) + 1);
        if (type->rescuer_type_name == NULL) {
            write_log("PARSER", "FILE_PARSING", "Memory allocation failed for rescuer name");
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        strcpy(type->rescuer_type_name, name);
        type->speed = speed;
        type->x = x;
        type->y = y;
        
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Parsed rescuer type: %s, count=%d, speed=%d, base=(%d,%d)", 
                 name, num, speed, x, y);
        write_log("PARSER", "FILE_PARSING", log_msg);
        
        // Create digital twins for this rescuer type
        for (int i = 0; i < num && rescuers_count < MAX_RESCUERS; i++) {
            rescuer_digital_twin_t *twin = &rescuers[rescuers_count];
            twin->id = rescuers_count;
            twin->x = x;
            twin->y = y;
            twin->rescuer = type;
            twin->status = IDLE;
            
            // Initialize mutex for this digital twin
            if (mtx_init(&twin->mutex, mtx_plain) != thrd_success) {
                write_log("PARSER", "FILE_PARSING", "Failed to initialize mutex for digital twin");
                exit(EXIT_FAILURE);
            }
            
            rescuers_count++;
            
            snprintf(log_msg, sizeof(log_msg), "Created digital twin %d for %s at (%d,%d)", 
                     twin->id, name, x, y);
            write_log("PARSER", "FILE_PARSING", log_msg);
        }
        
        rescuer_types_count++;
    }
    
    fclose(file);
    
    char completion_msg[128];
    snprintf(completion_msg, sizeof(completion_msg), 
             "Parsing completed. Types: %d, Total rescuers: %d", 
             rescuer_types_count, rescuers_count);
    write_log("PARSER", "FILE_PARSING", completion_msg);
}