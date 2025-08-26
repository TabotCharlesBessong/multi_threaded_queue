#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "macros.h"

// Global server state
server_state_t server_state;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nReceived SIGINT, initiating graceful shutdown...\n");
        server_state.shutdown_requested = 1;
    }
}

int main(int argc, char *argv[]) {
    printf("Emergency Management Server starting...\n");

    // Initialize server state
    memset(&server_state, 0, sizeof(server_state));
    server_state.shutdown_requested = 0;

    // Step 2: Parse configuration files
    printf("Loading configuration files...\n");

    if (parse_rescuers("rescuers.conf", &server_state.rescuers, &server_state.rescuer_count) != 0) {
        fprintf(stderr, "Failed to parse rescuers.conf\n");
        return 1;
    }

    if (parse_emergency_types("emergency_types.conf", &server_state.emergency_types, &server_state.emergency_type_count, 
                             server_state.rescuers, server_state.rescuer_count) != 0) {
        fprintf(stderr, "Failed to parse emergency_types.conf\n");
        free_rescuer_types(server_state.rescuers, server_state.rescuer_count);
        return 1;
    }

    if (parse_env("env.conf", server_state.queue_name, &server_state.max_emergencies, 
                  server_state.log_level, &server_state.base_timeout) != 0) {
        fprintf(stderr, "Failed to parse env.conf\n");
        free_rescuer_types(server_state.rescuers, server_state.rescuer_count);
        free_emergency_types(server_state.emergency_types, server_state.emergency_type_count);
        return 1;
    }

    // Display loaded configuration for verification
    printf("\n=== Configuration Summary ===\n");
    printf("Queue Name: %s\n", server_state.queue_name);
    printf("Max Emergencies: %d\n", server_state.max_emergencies);
    printf("Log Level: %s\n", server_state.log_level);
    printf("Base Timeout: %d seconds\n", server_state.base_timeout);
    
    printf("\nRescuer Types (%d):\n", server_state.rescuer_count);
    for (int i = 0; i < server_state.rescuer_count; i++) {
        printf("  %s: Speed=%d, Base=(%d,%d)\n", 
               server_state.rescuers[i].rescuer_type_name, server_state.rescuers[i].speed, 
               server_state.rescuers[i].base_x, server_state.rescuers[i].base_y);
    }
    
    printf("\nEmergency Types (%d):\n", server_state.emergency_type_count);
    for (int i = 0; i < server_state.emergency_type_count; i++) {
        printf("  %s (Priority: %d):\n", 
               server_state.emergency_types[i].emergency_desc, server_state.emergency_types[i].priority);
        for (int j = 0; j < server_state.emergency_types[i].rescuers_req_number; j++) {
            printf("    - %s: %d units for %d seconds\n",
                   server_state.emergency_types[i].rescuers[j].type->rescuer_type_name,
                   server_state.emergency_types[i].rescuers[j].required_count,
                   server_state.emergency_types[i].rescuers[j].time_to_manage);
        }
    }

    // Step 3: Initialize server components
    printf("\nInitializing server components...\n");
    if (initialize_server(&server_state) != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        cleanup_server(&server_state);
        free_rescuer_types(server_state.rescuers, server_state.rescuer_count);
        free_emergency_types(server_state.emergency_types, server_state.emergency_type_count);
        return 1;
    }

    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);

    // Create listener and dispatcher threads
    pthread_t listener_tid, dispatcher_tid;
    
    if (pthread_create(&listener_tid, NULL, listener_thread, &server_state) != 0) {
        perror("Failed to create listener thread");
        cleanup_server(&server_state);
        free_rescuer_types(server_state.rescuers, server_state.rescuer_count);
        free_emergency_types(server_state.emergency_types, server_state.emergency_type_count);
        return 1;
    }

    if (pthread_create(&dispatcher_tid, NULL, dispatcher_thread, &server_state) != 0) {
        perror("Failed to create dispatcher thread");
        server_state.shutdown_requested = 1;
        pthread_join(listener_tid, NULL);
        cleanup_server(&server_state);
        free_rescuer_types(server_state.rescuers, server_state.rescuer_count);
        free_emergency_types(server_state.emergency_types, server_state.emergency_type_count);
        return 1;
    }

    printf("Server threads started. Press Ctrl+C to shutdown.\n");

    // Wait for shutdown signal
    while (!server_state.shutdown_requested) {
        sleep(1);
    }

    printf("\nShutdown requested. Waiting for threads to finish...\n");

    // Wait for threads to finish
    pthread_join(listener_tid, NULL);
    pthread_join(dispatcher_tid, NULL);

    // Clean up
    cleanup_server(&server_state);
    free_rescuer_types(server_state.rescuers, server_state.rescuer_count);
    free_emergency_types(server_state.emergency_types, server_state.emergency_type_count);

    printf("Emergency Management Server shutdown complete.\n");
    return 0;
}