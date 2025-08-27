#include "common.h"

void parse_emergency_types(const char *filename) {
    FILE *file;
    char line[512];
    char name[64];
    int priority;
    
    write_log("PARSER", "FILE_PARSING", "Starting emergency_types.conf parsing");
    
    file = fopen(filename, "r");
    if (file == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to open %s", filename);
        write_log("PARSER", "FILE_PARSING", error_msg);
        perror("Error opening emergency_types.conf");
        exit(EXIT_FAILURE);
    }
    
    write_log("PARSER", "FILE_PARSING", "Successfully opened emergency_types.conf");
    
    emergency_types_count = 0;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        // Check if we have space for more emergency types
        if (emergency_types_count >= MAX_EMERGENCY_TYPES) {
            write_log("PARSER", "FILE_PARSING", "Maximum emergency types exceeded");
            break;
        }
        
        // Find the pattern [Name][Priority] and extract rescuer requirements
        char *bracket1 = strchr(line, '[');
        char *bracket2 = strchr(bracket1 + 1, ']');
        char *bracket3 = strchr(bracket2 + 1, '[');
        char *bracket4 = strchr(bracket3 + 1, ']');
        
        if (!bracket1 || !bracket2 || !bracket3 || !bracket4) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Invalid format in line: %s", line);
            write_log("PARSER", "FILE_PARSING", log_msg);
            continue;
        }
        
        // Extract name
        int name_len = bracket2 - bracket1 - 1;
        strncpy(name, bracket1 + 1, name_len);
        name[name_len] = '\0';
        
        // Extract priority
        priority = atoi(bracket3 + 1);
        
        // Validate priority
        if (priority < 0 || priority > 2) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Invalid priority %d for emergency %s", priority, name);
            write_log("PARSER", "FILE_PARSING", log_msg);
            continue;
        }
        
        emergency_type_t *type = &emergency_types[emergency_types_count];
        type->emergency_desc = malloc(strlen(name) + 1);
        if (type->emergency_desc == NULL) {
            write_log("PARSER", "FILE_PARSING", "Memory allocation failed for emergency name");
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        strcpy(type->emergency_desc, name);
        type->priority = priority;
        
        // Parse rescuer requirements after the second ]
        char *rescuer_section = bracket4 + 1;
        
        // Count rescuer requirements first
        int req_count = 0;
        char *temp_ptr = rescuer_section;
        while ((temp_ptr = strchr(temp_ptr, ':')) != NULL) {
            req_count++;
            temp_ptr++;
        }
        
        type->rescuers_req_number = req_count;
        type->rescuers = malloc(req_count * sizeof(rescuer_request_t));
        if (type->rescuers == NULL) {
            write_log("PARSER", "FILE_PARSING", "Memory allocation failed for rescuer requests");
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        
        // Parse each rescuer requirement
        int req_index = 0;
        char *token = strtok(rescuer_section, ";");
        while (token != NULL && req_index < req_count) {
            char rescuer_name[64];
            int count, time;
            
            if (sscanf(token, "%63[^:]:%d,%d", rescuer_name, &count, &time) == 3) {
                rescuer_type_t *rescuer_type = find_rescuer_type(rescuer_name);
                if (rescuer_type == NULL) {
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "Unknown rescuer type: %s", rescuer_name);
                    write_log("PARSER", "FILE_PARSING", log_msg);
                    token = strtok(NULL, ";");
                    continue;
                }
                
                type->rescuers[req_index].type = rescuer_type;
                type->rescuers[req_index].required_count = count;
                type->rescuers[req_index].time_to_manage = time;
                req_index++;
                
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Added requirement: %s x%d for %ds", 
                         rescuer_name, count, time);
                write_log("PARSER", "FILE_PARSING", log_msg);
            } else {
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Invalid rescuer format in token: %s", token);
                write_log("PARSER", "FILE_PARSING", log_msg);
            }
            
            token = strtok(NULL, ";");
        }
        
        type->rescuers_req_number = req_index; // Update with actual parsed count
        
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Parsed emergency type: %s, priority=%d, rescuer_reqs=%d", 
                 name, priority, req_index);
        write_log("PARSER", "FILE_PARSING", log_msg);
        
        emergency_types_count++;
    }
    
    fclose(file);
    
    char completion_msg[128];
    snprintf(completion_msg, sizeof(completion_msg), 
             "Emergency types parsing completed. Total types: %d", 
             emergency_types_count);
    write_log("PARSER", "FILE_PARSING", completion_msg);
}