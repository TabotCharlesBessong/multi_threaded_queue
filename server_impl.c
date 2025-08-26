#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
#include "macros.h"

// =============================================================================
// QUEUE OPERATIONS
// =============================================================================

void init_priority_queue(priority_queue_t* queue) {
    queue->head = NULL;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
}

void enqueue_emergency(priority_queue_t* queue, emergency_t* emergency) {
    pthread_mutex_lock(&queue->mutex);
    
    queue_node_t* new_node = malloc(sizeof(queue_node_t));
    if (new_node == NULL) {
        perror("Failed to allocate queue node");
        pthread_mutex_unlock(&queue->mutex);
        return;
    }
    
    new_node->emergency = emergency;
    new_node->next = queue->head;
    queue->head = new_node;
    queue->count++;
    
    pthread_mutex_unlock(&queue->mutex);
}

emergency_t* dequeue_emergency(priority_queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    queue_node_t* node = queue->head;
    emergency_t* emergency = node->emergency;
    queue->head = node->next;
    queue->count--;
    
    free(node);
    pthread_mutex_unlock(&queue->mutex);
    
    return emergency;
}

void cleanup_priority_queue(priority_queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    queue_node_t* current = queue->head;
    while (current != NULL) {
        queue_node_t* next = current->next;
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->count = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
}

// =============================================================================
// RESCUER POOL MANAGEMENT
// =============================================================================

int create_rescuer_pool(server_state_t* state) {
    // Calculate total number of rescuers needed
    int total_rescuers = 0;
    for (int i = 0; i < state->rescuer_count; i++) {
        // Create 3 instances of each rescuer type
        total_rescuers += 3;
    }
    
    state->rescuer_pool_size = total_rescuers;
    state->rescuer_pool = malloc(total_rescuers * sizeof(rescuer_digital_twin_t));
    if (state->rescuer_pool == NULL) {
        perror("Failed to allocate rescuer pool");
        return -1;
    }
    
    int rescuer_index = 0;
    for (int i = 0; i < state->rescuer_count; i++) {
        for (int j = 0; j < 3; j++) {
            state->rescuer_pool[rescuer_index].id = rescuer_index;
            state->rescuer_pool[rescuer_index].current_x = state->rescuers[i].base_x;
            state->rescuer_pool[rescuer_index].current_y = state->rescuers[i].base_y;
            state->rescuer_pool[rescuer_index].rescuer_type = &state->rescuers[i];
            state->rescuer_pool[rescuer_index].status = IDLE;
            rescuer_index++;
        }
    }
    
    pthread_mutex_init(&state->rescuer_pool_mutex, NULL);
    printf("Created rescuer pool with %d units\n", total_rescuers);
    return 0;
}

// =============================================================================
// SERVER INITIALIZATION AND CLEANUP
// =============================================================================

int initialize_server(server_state_t* state) {
    // Initialize priority queues
    init_priority_queue(&state->high_priority_q);
    init_priority_queue(&state->medium_priority_q);
    init_priority_queue(&state->low_priority_q);
    
    // Create rescuer pool
    if (create_rescuer_pool(state) != 0) {
        return -1;
    }
    
    printf("Server initialized successfully\n");
    return 0;
}

void cleanup_server(server_state_t* state) {
    printf("Cleaning up server resources...\n");
    
    // Cleanup priority queues
    cleanup_priority_queue(&state->high_priority_q);
    cleanup_priority_queue(&state->medium_priority_q);
    cleanup_priority_queue(&state->low_priority_q);
    
    // Cleanup rescuer pool
    if (state->rescuer_pool != NULL) {
        pthread_mutex_destroy(&state->rescuer_pool_mutex);
        free(state->rescuer_pool);
    }
    
    printf("Server cleanup completed\n");
}

// =============================================================================
// THREAD FUNCTIONS
// =============================================================================

void* listener_thread(void* arg) {
    server_state_t* state = (server_state_t*)arg;
    
    printf("Listener thread started\n");
    
    // For demonstration purposes, create some test emergencies
    // In a real implementation, this would receive from a message queue or socket
    
    // Create a test high priority emergency
    emergency_t* emergency1 = malloc(sizeof(emergency_t));
    if (emergency1 != NULL) {
        emergency1->id = 1;
        emergency1->type = &state->emergency_types[0]; // Incendio (priority 3)
        emergency1->status = WAITING;
        emergency1->x = 150;
        emergency1->y = 200;
        emergency1->time = time(NULL);
        emergency1->rescuer_count = 0;
        emergency1->rescuers_dt = NULL;
        
        enqueue_emergency(&state->high_priority_q, emergency1);
        printf("Test: Enqueued high priority emergency (Incendio)\n");
    }
    
    // Wait a bit, then create a medium priority emergency
    sleep(2);
    
    emergency_t* emergency2 = malloc(sizeof(emergency_t));
    if (emergency2 != NULL) {
        emergency2->id = 2;
        emergency2->type = &state->emergency_types[1]; // Incidente_Stradale (priority 2)
        emergency2->status = WAITING;
        emergency2->x = 100;
        emergency2->y = 150;
        emergency2->time = time(NULL);
        emergency2->rescuer_count = 0;
        emergency2->rescuers_dt = NULL;
        
        enqueue_emergency(&state->medium_priority_q, emergency2);
        printf("Test: Enqueued medium priority emergency (Incidente_Stradale)\n");
    }
    
    // Wait for shutdown
    while (!state->shutdown_requested) {
        sleep(1);
    }
    
    printf("Listener thread shutting down\n");
    return NULL;
}

void* dispatcher_thread(void* arg) {
    server_state_t* state = (server_state_t*)arg;
    
    printf("Dispatcher thread started\n");
    
    while (!state->shutdown_requested) {
        emergency_t* emergency = NULL;
        
        // Check high priority queue first
        emergency = dequeue_emergency(&state->high_priority_q);
        if (emergency != NULL) {
            printf("Processing high priority emergency: %s\n", emergency->type->emergency_desc);
            // TODO: Create worker thread for this emergency
            // For now, just log it
            emergency->status = ASSIGNED;
            printf("Emergency %s assigned (would create worker thread)\n", emergency->type->emergency_desc);
            free(emergency);
            continue;
        }
        
        // Check medium priority queue
        emergency = dequeue_emergency(&state->medium_priority_q);
        if (emergency != NULL) {
            printf("Processing medium priority emergency: %s\n", emergency->type->emergency_desc);
            emergency->status = ASSIGNED;
            printf("Emergency %s assigned (would create worker thread)\n", emergency->type->emergency_desc);
            free(emergency);
            continue;
        }
        
        // Check low priority queue
        emergency = dequeue_emergency(&state->low_priority_q);
        if (emergency != NULL) {
            printf("Processing low priority emergency: %s\n", emergency->type->emergency_desc);
            emergency->status = ASSIGNED;
            printf("Emergency %s assigned (would create worker thread)\n", emergency->type->emergency_desc);
            free(emergency);
            continue;
        }
        
        // All queues empty, sleep to prevent busy waiting
        usleep(100000); // 100ms
    }
    
    printf("Dispatcher thread shutting down\n");
    return NULL;
}

void* worker_thread(void* arg) {
    // TODO: Implement worker thread logic
    // This will be implemented in Step 4
    return NULL;
}
