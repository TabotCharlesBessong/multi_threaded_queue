#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "macros.h"

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

rescuer_type_t* find_rescuer_type(const char* name, rescuer_type_t* rescuers, int rescuer_count) {
    for (int i = 0; i < rescuer_count; i++) {
        if (strcmp(rescuers[i].rescuer_type_name, name) == 0) {
            return &rescuers[i];
        }
    }
    return NULL;
}

void free_rescuer_types(rescuer_type_t* rescuers, int count) {
    if (rescuers == NULL) return;
    
    for (int i = 0; i < count; i++) {
        if (rescuers[i].rescuer_type_name != NULL) {
            free(rescuers[i].rescuer_type_name);
        }
    }
    free(rescuers);
}

void free_emergency_types(emergency_type_t* emergency_types, int count) {
    if (emergency_types == NULL) return;
    
    for (int i = 0; i < count; i++) {
        if (emergency_types[i].emergency_desc != NULL) {
            free(emergency_types[i].emergency_desc);
        }
        if (emergency_types[i].rescuers != NULL) {
            free(emergency_types[i].rescuers);
        }
    }
    free(emergency_types);
}

// =============================================================================
// PARSER FUNCTIONS
// =============================================================================

int parse_rescuers(const char* filename, rescuer_type_t** rescuers, int* rescuer_count) {
    FILE* file;
    char line[256];
    int count = 0;
    int capacity = 10;
    
    // Initialize the rescuers array
    *rescuers = malloc(capacity * sizeof(rescuer_type_t));
    if (*rescuers == NULL) {
        perror("Failed to allocate memory for rescuers");
        return -1;
    }
    
    SNCALL(file, fopen(filename, "r"), "Failed to open rescuers.conf");
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        char name[64];
        int speed, base_x, base_y;
        
        if (sscanf(line, "%63s %d %d %d", name, &speed, &base_x, &base_y) == 4) {
            // Expand capacity if needed
            if (count >= capacity) {
                capacity *= 2;
                rescuer_type_t* new_rescuers = realloc(*rescuers, capacity * sizeof(rescuer_type_t));
                if (new_rescuers == NULL) {
                    perror("Failed to reallocate memory for rescuers");
                    fclose(file);
                    return -1;
                }
                *rescuers = new_rescuers;
            }
            
            // Allocate and copy the name
            (*rescuers)[count].rescuer_type_name = strdup(name);
            if ((*rescuers)[count].rescuer_type_name == NULL) {
                perror("Failed to allocate memory for rescuer name");
                fclose(file);
                return -1;
            }
            
            (*rescuers)[count].speed = speed;
            (*rescuers)[count].base_x = base_x;
            (*rescuers)[count].base_y = base_y;
            
            count++;
        }
    }
    
    fclose(file);
    *rescuer_count = count;
    
    // Trim the array to exact size
    if (count > 0) {
        rescuer_type_t* trimmed = realloc(*rescuers, count * sizeof(rescuer_type_t));
        if (trimmed != NULL) {
            *rescuers = trimmed;
        }
    }
    
    printf("Parsed %d rescuer types from %s\n", count, filename);
    return 0;
}

int parse_emergency_types(const char* filename, emergency_type_t** emergency_types, int* emergency_type_count, 
                         rescuer_type_t* rescuers, int rescuer_count) {
    FILE* file;
    char line[512];
    int count = 0;
    int capacity = 10;
    
    // Initialize the emergency types array
    *emergency_types = malloc(capacity * sizeof(emergency_type_t));
    if (*emergency_types == NULL) {
        perror("Failed to allocate memory for emergency types");
        return -1;
    }
    
    SNCALL(file, fopen(filename, "r"), "Failed to open emergency_types.conf");
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        char desc[64];
        short priority;
        
        if (sscanf(line, "%63s %hd", desc, &priority) == 2) {
            // Expand capacity if needed
            if (count >= capacity) {
                capacity *= 2;
                emergency_type_t* new_types = realloc(*emergency_types, capacity * sizeof(emergency_type_t));
                if (new_types == NULL) {
                    perror("Failed to reallocate memory for emergency types");
                    fclose(file);
                    return -1;
                }
                *emergency_types = new_types;
            }
            
            // Allocate and copy the description
            (*emergency_types)[count].emergency_desc = strdup(desc);
            if ((*emergency_types)[count].emergency_desc == NULL) {
                perror("Failed to allocate memory for emergency description");
                fclose(file);
                return -1;
            }
            
            (*emergency_types)[count].priority = priority;
            (*emergency_types)[count].rescuers = NULL;
            (*emergency_types)[count].rescuers_req_number = 0;
            
            // Parse rescuer requirements (variable number of rescuers per emergency)
            char* token = strtok(line, " \t\n");
            token = strtok(NULL, " \t\n"); // Skip description
            token = strtok(NULL, " \t\n"); // Skip priority
            
            int rescuer_capacity = 5;
            (*emergency_types)[count].rescuers = malloc(rescuer_capacity * sizeof(rescuer_request_t));
            
            while (token != NULL) {
                char rescuer_name[64];
                int required_count, time_to_manage;
                
                if (sscanf(token, "%63s", rescuer_name) == 1) {
                    token = strtok(NULL, " \t\n");
                    if (token != NULL && sscanf(token, "%d", &required_count) == 1) {
                        token = strtok(NULL, " \t\n");
                        if (token != NULL && sscanf(token, "%d", &time_to_manage) == 1) {
                            // Expand rescuer capacity if needed
                            if ((*emergency_types)[count].rescuers_req_number >= rescuer_capacity) {
                                rescuer_capacity *= 2;
                                rescuer_request_t* new_rescuers = realloc((*emergency_types)[count].rescuers, 
                                                                       rescuer_capacity * sizeof(rescuer_request_t));
                                if (new_rescuers == NULL) {
                                    perror("Failed to reallocate memory for rescuer requests");
                                    fclose(file);
                                    return -1;
                                }
                                (*emergency_types)[count].rescuers = new_rescuers;
                            }
                            
                            // Find the rescuer type
                            rescuer_type_t* rescuer_type = find_rescuer_type(rescuer_name, rescuers, rescuer_count);
                            if (rescuer_type != NULL) {
                                int idx = (*emergency_types)[count].rescuers_req_number;
                                (*emergency_types)[count].rescuers[idx].type = rescuer_type;
                                (*emergency_types)[count].rescuers[idx].required_count = required_count;
                                (*emergency_types)[count].rescuers[idx].time_to_manage = time_to_manage;
                                (*emergency_types)[count].rescuers_req_number++;
                            } else {
                                printf("Warning: Unknown rescuer type '%s' for emergency '%s'\n", 
                                       rescuer_name, desc);
                            }
                        }
                    }
                }
                token = strtok(NULL, " \t\n");
            }
            
            // Trim the rescuers array to exact size
            if ((*emergency_types)[count].rescuers_req_number > 0) {
                rescuer_request_t* trimmed = realloc((*emergency_types)[count].rescuers, 
                                                   (*emergency_types)[count].rescuers_req_number * sizeof(rescuer_request_t));
                if (trimmed != NULL) {
                    (*emergency_types)[count].rescuers = trimmed;
                }
            }
            
            count++;
        }
    }
    
    fclose(file);
    *emergency_type_count = count;
    
    // Trim the array to exact size
    if (count > 0) {
        emergency_type_t* trimmed = realloc(*emergency_types, count * sizeof(emergency_type_t));
        if (trimmed != NULL) {
            *emergency_types = trimmed;
        }
    }
    
    printf("Parsed %d emergency types from %s\n", count, filename);
    return 0;
}

int parse_env(const char* filename, char* queue_name, int* max_emergencies, char* log_level, int* base_timeout) {
    FILE* file;
    char line[256];
    char key[64], value[192];
    
    // Set default values
    strcpy(queue_name, "/emergency_queue");
    *max_emergencies = 100;
    strcpy(log_level, "INFO");
    *base_timeout = 300;
    
    SNCALL(file, fopen(filename, "r"), "Failed to open env.conf");
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        if (sscanf(line, "%63s %191s", key, value) == 2) {
            if (strcmp(key, "QUEUE_NAME") == 0) {
                strcpy(queue_name, value);
            } else if (strcmp(key, "MAX_EMERGENCIES") == 0) {
                *max_emergencies = atoi(value);
            } else if (strcmp(key, "LOG_LEVEL") == 0) {
                strcpy(log_level, value);
            } else if (strcmp(key, "BASE_TIMEOUT") == 0) {
                *base_timeout = atoi(value);
            }
        }
    }
    
    fclose(file);
    printf("Parsed environment configuration from %s\n", filename);
    return 0;
}