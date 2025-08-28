#include "common.h"

// Global variable definitions
rescuer_type_t rescuer_types[MAX_RESCUER_TYPES];
int rescuer_types_count = 0;
rescuer_digital_twin_t rescuers[MAX_RESCUERS];
int rescuers_count = 0;
emergency_type_t emergency_types[MAX_EMERGENCY_TYPES];
int emergency_types_count = 0;
environment_config_t env_config;
FILE *log_file = NULL;
mtx_t log_mutex;
emergency_queue_t priority_queues[3];

void init_logging(void) {
    if (mtx_init(&log_mutex, mtx_plain) != thrd_success) {
        fprintf(stderr, "Failed to initialize log mutex\n");
        exit(EXIT_FAILURE);
    }
    
    log_file = fopen(LOG_FILENAME, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    
    // Write session start marker
    time_t now = time(NULL);
    fprintf(log_file, "\n=== Emergency System Session Started [%ld] ===\n", (long)now);
    fflush(log_file);
}

void cleanup_logging(void) {
    if (log_file != NULL) {
        time_t now = time(NULL);
        fprintf(log_file, "=== Emergency System Session Ended [%ld] ===\n\n", (long)now);
        fclose(log_file);
        log_file = NULL;
    }
    mtx_destroy(&log_mutex);
}

void write_log(const char *id, const char *event, const char *message) {
    if (log_file == NULL) return;
    
    mtx_lock(&log_mutex);
    time_t now = time(NULL);
    fprintf(log_file, "[%ld] [%s] [%s] %s\n", (long)now, id, event, message);
    fflush(log_file);
    mtx_unlock(&log_mutex);
}

int calculate_manhattan_distance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

double calculate_travel_time(int distance, int speed) {
    if (speed <= 0) return 0.0;
    return (double)distance / (double)speed;
}

rescuer_type_t* find_rescuer_type(const char *name) {
    for (int i = 0; i < rescuer_types_count; i++) {
        if (strcmp(rescuer_types[i].rescuer_type_name, name) == 0) {
            return &rescuer_types[i];
        }
    }
    return NULL;
}

emergency_type_t* find_emergency_type(const char *name) {
    for (int i = 0; i < emergency_types_count; i++) {
        if (strcmp(emergency_types[i].emergency_desc, name) == 0) {
            return &emergency_types[i];
        }
    }
    return NULL;
}

// Emergency queue operations
void init_emergency_queue(emergency_queue_t *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    
    if (mtx_init(&queue->mutex, mtx_plain) != thrd_success) {
        write_log("SYSTEM", "QUEUE_INIT", "Failed to initialize queue mutex");
        exit(EXIT_FAILURE);
    }
    
    if (cnd_init(&queue->condition) != thrd_success) {
        write_log("SYSTEM", "QUEUE_INIT", "Failed to initialize queue condition variable");
        exit(EXIT_FAILURE);
    }
}

void enqueue_emergency(emergency_queue_t *queue, emergency_t *emergency) {
    emergency_queue_node_t *new_node = malloc(sizeof(emergency_queue_node_t));
    if (new_node == NULL) {
        write_log("SYSTEM", "QUEUE_ERROR", "Failed to allocate memory for queue node");
        return;
    }
    
    new_node->emergency = emergency;
    new_node->next = NULL;
    
    mtx_lock(&queue->mutex);
    
    if (queue->tail == NULL) {
        queue->head = queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    
    queue->count++;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Emergency %d enqueued, queue size: %d", 
             emergency->id, queue->count);
    write_log("QUEUE", "EMERGENCY_ENQUEUE", log_msg);
    
    cnd_signal(&queue->condition);
    mtx_unlock(&queue->mutex);
}

// Fixed dequeue function in utils.c - don't lock in dispatcher since we already lock there

emergency_t* dequeue_emergency(emergency_queue_t *queue) {
    // Note: This function expects the mutex to already be locked by the caller
    
    if (queue->head == NULL) {
        return NULL;
    }
    
    emergency_queue_node_t *node = queue->head;
    emergency_t *emergency = node->emergency;
    
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    
    queue->count--;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Emergency %d dequeued, queue size: %d", 
             emergency->id, queue->count);
    write_log("QUEUE", "EMERGENCY_DEQUEUE", log_msg);
    
    free(node);
    
    return emergency;
}

// Alternative: Create a safer version that handles its own locking
emergency_t* dequeue_emergency_safe(emergency_queue_t *queue) {
    mtx_lock(&queue->mutex);
    
    if (queue->head == NULL) {
        mtx_unlock(&queue->mutex);
        return NULL;
    }
    
    emergency_queue_node_t *node = queue->head;
    emergency_t *emergency = node->emergency;
    
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    
    queue->count--;
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Emergency %d dequeued, queue size: %d", 
             emergency->id, queue->count);
    write_log("QUEUE", "EMERGENCY_DEQUEUE", log_msg);
    
    free(node);
    mtx_unlock(&queue->mutex);
    
    return emergency;
}

void cleanup_emergency_queue(emergency_queue_t *queue) {
    mtx_lock(&queue->mutex);
    
    while (queue->head != NULL) {
        emergency_queue_node_t *node = queue->head;
        queue->head = node->next;
        
        // Free the emergency and its resources
        if (node->emergency->rescuers_dt != NULL) {
            free(node->emergency->rescuers_dt);
        }
        mtx_destroy(&node->emergency->mutex);
        free(node->emergency);
        free(node);
    }
    
    queue->tail = NULL;
    queue->count = 0;
    
    mtx_unlock(&queue->mutex);
    mtx_destroy(&queue->mutex);
    cnd_destroy(&queue->condition);
}

// Status string conversion functions for logging
const char* rescuer_status_to_string(rescuer_status_t status) {
    switch (status) {
        case IDLE: return "IDLE";
        case EN_ROUTE_TO_SCENE: return "EN_ROUTE_TO_SCENE";
        case ON_SCENE: return "ON_SCENE";
        case RETURNING_TO_BASE: return "RETURNING_TO_BASE";
        default: return "UNKNOWN";
    }
}

const char* emergency_status_to_string(emergency_status_t status) {
    switch (status) {
        case WAITING: return "WAITING";
        case ASSIGNED: return "ASSIGNED";
        case IN_PROGRESS: return "IN_PROGRESS";
        case PAUSED: return "PAUSED";
        case COMPLETED: return "COMPLETED";
        case CANCELED: return "CANCELED";
        case TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}