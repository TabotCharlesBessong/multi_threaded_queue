#include "common.h"
#include <sys/select.h>

static mqd_t message_queue = -1;
static volatile sig_atomic_t terminate_flag = 0;
static thrd_t dispatcher_thread;
static thrd_t *worker_threads;
static int worker_count = 0;
static int next_emergency_id = 1;
static mtx_t emergency_id_mutex;

typedef struct {
    emergency_t *emergency;
    int worker_id;
} worker_thread_data_t;

void cleanup_server(void) {
    write_log("SERVER", "SHUTDOWN", "Starting server cleanup");
    
    // Signal termination to all threads
    terminate_flag = 1;
    
    // Wake up dispatcher and workers
    for (int i = 0; i < 3; i++) {
        mtx_lock(&priority_queues[i].mutex);
        cnd_broadcast(&priority_queues[i].condition);
        mtx_unlock(&priority_queues[i].mutex);
    }
    
    // Wait for dispatcher to finish
    if (dispatcher_thread) {
        thrd_join(dispatcher_thread, NULL);
        write_log("SERVER", "SHUTDOWN", "Dispatcher thread joined");
    }
    
    // Clean up message queue
    if (message_queue != -1) {
        mq_close(message_queue);
        mq_unlink(env_config.queue_name);
        write_log("SERVER", "MESSAGE_QUEUE", "Message queue closed and unlinked");
    }
    
    // Clean up emergency queues
    for (int i = 0; i < 3; i++) {
        cleanup_emergency_queue(&priority_queues[i]);
    }
    
    // Clean up rescuer mutexes
    for (int i = 0; i < rescuers_count; i++) {
        mtx_destroy(&rescuers[i].mutex);
    }
    
    mtx_destroy(&emergency_id_mutex);
    
    // Free allocated memory
    for (int i = 0; i < rescuer_types_count; i++) {
        free(rescuer_types[i].rescuer_type_name);
    }
    
    for (int i = 0; i < emergency_types_count; i++) {
        free(emergency_types[i].emergency_desc);
        free(emergency_types[i].rescuers);
    }
    
    if (worker_threads) {
        free(worker_threads);
    }
    
    cleanup_logging();
    printf("Server shutdown complete.\n");
}

void sigint_handler(int sig) {
    terminate_flag = 1;
    const char msg[] = "\nServer termination requested...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int get_next_emergency_id(void) {
    mtx_lock(&emergency_id_mutex);
    int id = next_emergency_id++;
    mtx_unlock(&emergency_id_mutex);
    return id;
}

emergency_t* create_emergency_from_request(emergency_request_t *request) {
    char log_msg[256];
    
    // Validate emergency type
    emergency_type_t *type = find_emergency_type(request->emergency_name);
    if (type == NULL) {
        snprintf(log_msg, sizeof(log_msg), "Unknown emergency type: %s", request->emergency_name);
        write_log("SERVER", "VALIDATION", log_msg);
        return NULL;
    }
    
    // Validate coordinates
    if (request->x < 0 || request->x >= env_config.width || 
        request->y < 0 || request->y >= env_config.height) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Invalid coordinates (%d,%d) for emergency %s", 
                 request->x, request->y, request->emergency_name);
        write_log("SERVER", "VALIDATION", log_msg);
        return NULL;
    }
    
    // Create emergency
    emergency_t *emergency = malloc(sizeof(emergency_t));
    if (emergency == NULL) {
        write_log("SERVER", "MEMORY", "Failed to allocate memory for emergency");
        return NULL;
    }
    
    emergency->type = *type;
    emergency->status = WAITING;
    emergency->x = request->x;
    emergency->y = request->y;
    emergency->time = request->timestamp;
    emergency->rescuer_count = 0;
    emergency->rescuers_dt = NULL;
    emergency->id = get_next_emergency_id();
    
    if (mtx_init(&emergency->mutex, mtx_plain) != thrd_success) {
        write_log("SERVER", "MUTEX", "Failed to initialize emergency mutex");
        free(emergency);
        return NULL;
    }
    
    // Calculate total rescuers needed
    int total_rescuers = 0;
    for (int i = 0; i < type->rescuers_req_number; i++) {
        total_rescuers += type->rescuers[i].required_count;
    }
    
    if (total_rescuers > 0) {
        emergency->rescuers_dt = malloc(total_rescuers * sizeof(rescuer_digital_twin_t*));
        if (emergency->rescuers_dt == NULL) {
            write_log("SERVER", "MEMORY", "Failed to allocate memory for emergency rescuers");
            mtx_destroy(&emergency->mutex);
            free(emergency);
            return NULL;
        }
    }
    
    snprintf(log_msg, sizeof(log_msg), 
             "Created emergency %d: %s at (%d,%d) priority=%d", 
             emergency->id, request->emergency_name, request->x, request->y, type->priority);
    write_log("SERVER", "EMERGENCY_CREATION", log_msg);
    
    return emergency;
}

int assign_rescuers_to_emergency(emergency_t *emergency) {
    char log_msg[256];
    int assigned_count = 0;
    
    snprintf(log_msg, sizeof(log_msg), "Attempting to assign rescuers to emergency %d", emergency->id);
    write_log("SERVER", "RESCUER_ASSIGNMENT", log_msg);
    
    // Try to assign all required rescuer types
    for (int req_idx = 0; req_idx < emergency->type.rescuers_req_number; req_idx++) {
        rescuer_request_t *req = &emergency->type.rescuers[req_idx];
        int found_count = 0;
        
        // Find available rescuers of this type
        for (int i = 0; i < rescuers_count && found_count < req->required_count; i++) {
            rescuer_digital_twin_t *rescuer = &rescuers[i];
            
            mtx_lock(&rescuer->mutex);
            
            if (rescuer->rescuer == req->type && rescuer->status == IDLE) {
                emergency->rescuers_dt[assigned_count] = rescuer;
                rescuer->status = ASSIGNED;
                assigned_count++;
                found_count++;
                
                snprintf(log_msg, sizeof(log_msg), 
                         "Assigned rescuer %d (%s) to emergency %d", 
                         rescuer->id, rescuer->rescuer->rescuer_type_name, emergency->id);
                write_log("SERVER", "RESCUER_STATUS", log_msg);
            }
            
            mtx_unlock(&rescuer->mutex);
        }
        
        // Check if we found enough rescuers of this type
        if (found_count < req->required_count) {
            snprintf(log_msg, sizeof(log_msg), 
                     "Insufficient rescuers: need %d %s, found %d for emergency %d", 
                     req->required_count, req->type->rescuer_type_name, found_count, emergency->id);
            write_log("SERVER", "RESCUER_ASSIGNMENT", log_msg);
            
            // Release already assigned rescuers
            for (int j = 0; j < assigned_count; j++) {
                mtx_lock(&emergency->rescuers_dt[j]->mutex);
                emergency->rescuers_dt[j]->status = IDLE;
                mtx_unlock(&emergency->rescuers_dt[j]->mutex);
            }
            
            return 0; // Assignment failed
        }
    }
    
    emergency->rescuer_count = assigned_count;
    emergency->status = ASSIGNED;
    
    snprintf(log_msg, sizeof(log_msg), 
             "Successfully assigned %d rescuers to emergency %d", 
             assigned_count, emergency->id);
    write_log("SERVER", "RESCUER_ASSIGNMENT", log_msg);
    
    return 1; // Assignment successful
}

void simulate_emergency_response(emergency_t *emergency) {
    char log_msg[256];
    
    // Change status to IN_PROGRESS
    mtx_lock(&emergency->mutex);
    emergency->status = IN_PROGRESS;
    mtx_unlock(&emergency->mutex);
    
    snprintf(log_msg, sizeof(log_msg), "Emergency %d status: IN_PROGRESS", emergency->id);
    write_log("SERVER", "EMERGENCY_STATUS", log_msg);
    
    // Move rescuers to scene
    for (int i = 0; i < emergency->rescuer_count; i++) {
        rescuer_digital_twin_t *rescuer = emergency->rescuers_dt[i];
        
        mtx_lock(&rescuer->mutex);
        rescuer->status = EN_ROUTE_TO_SCENE;
        mtx_unlock(&rescuer->mutex);
        
        // Calculate travel time
        int distance = calculate_manhattan_distance(rescuer->x, rescuer->y, emergency->x, emergency->y);
        double travel_time = calculate_travel_time(distance, rescuer->rescuer->speed);
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Rescuer %d en route to emergency %d, ETA: %.2f seconds", 
                 rescuer->id, emergency->id, travel_time);
        write_log("SERVER", "RESCUER_STATUS", log_msg);
        
        // Simulate travel time
        struct timespec sleep_time;
        sleep_time.tv_sec = (time_t)travel_time;
        sleep_time.tv_nsec = (long)((travel_time - sleep_time.tv_sec) * 1000000000);
        thrd_sleep(&sleep_time, NULL);
        
        if (terminate_flag) return;
        
        // Arrive at scene
        mtx_lock(&rescuer->mutex);
        rescuer->status = ON_SCENE;
        rescuer->x = emergency->x;
        rescuer->y = emergency->y;
        mtx_unlock(&rescuer->mutex);
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Rescuer %d arrived at emergency %d scene", rescuer->id, emergency->id);
        write_log("SERVER", "RESCUER_STATUS", log_msg);
    }
    
    // Find the longest intervention time
    int max_intervention_time = 0;
    for (int req_idx = 0; req_idx < emergency->type.rescuers_req_number; req_idx++) {
        if (emergency->type.rescuers[req_idx].time_to_manage > max_intervention_time) {
            max_intervention_time = emergency->type.rescuers[req_idx].time_to_manage;
        }
    }
    
    snprintf(log_msg, sizeof(log_msg), 
             "Emergency %d intervention time: %d seconds", emergency->id, max_intervention_time);
    write_log("SERVER", "EMERGENCY_STATUS", log_msg);
    
    // Simulate intervention
    sleep(max_intervention_time);
    
    if (terminate_flag) return;
    
    // Complete emergency
    mtx_lock(&emergency->mutex);
    emergency->status = COMPLETED;
    mtx_unlock(&emergency->mutex);
    
    snprintf(log_msg, sizeof(log_msg), "Emergency %d status: COMPLETED", emergency->id);
    write_log("SERVER", "EMERGENCY_STATUS", log_msg);
    
    // Send rescuers back to base
    for (int i = 0; i < emergency->rescuer_count; i++) {
        rescuer_digital_twin_t *rescuer = emergency->rescuers_dt[i];
        
        mtx_lock(&rescuer->mutex);
        rescuer->status = RETURNING_TO_BASE;
        mtx_unlock(&rescuer->mutex);
        
        // Calculate return travel time
        int distance = calculate_manhattan_distance(rescuer->x, rescuer->y, 
                                                  rescuer->rescuer->x, rescuer->rescuer->y);
        double travel_time = calculate_travel_time(distance, rescuer->rescuer->speed);
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Rescuer %d returning to base, ETA: %.2f seconds", 
                 rescuer->id, travel_time);
        write_log("SERVER", "RESCUER_STATUS", log_msg);
        
        // Simulate return travel
        struct timespec sleep_time;
        sleep_time.tv_sec = (time_t)travel_time;
        sleep_time.tv_nsec = (long)((travel_time - sleep_time.tv_sec) * 1000000000);
        thrd_sleep(&sleep_time, NULL);
        
        if (terminate_flag) break;
        
        // Arrive at base
        mtx_lock(&rescuer->mutex);
        rescuer->status = IDLE;
        rescuer->x = rescuer->rescuer->x;
        rescuer->y = rescuer->rescuer->y;
        mtx_unlock(&rescuer->mutex);
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Rescuer %d returned to base", rescuer->id);
        write_log("SERVER", "RESCUER_STATUS", log_msg);
    }
}

int worker_thread_func(void *arg) {
    worker_thread_data_t *data = (worker_thread_data_t*)arg;
    emergency_t *emergency = data->emergency;
    int worker_id = data->worker_id;
    char log_msg[256];
    
    snprintf(log_msg, sizeof(log_msg), "Worker %d handling emergency %d", worker_id, emergency->id);
    write_log("SERVER", "WORKER_THREAD", log_msg);
    
    // Check timeout conditions
    time_t now = time(NULL);
    int timeout_seconds = 0;
    
    switch (emergency->type.priority) {
        case 0: timeout_seconds = 0; break;      // No timeout
        case 1: timeout_seconds = 30; break;     // Medium priority: 30 seconds
        case 2: timeout_seconds = 10; break;     // High priority: 10 seconds
    }
    
    if (timeout_seconds > 0 && (now - emergency->time) > timeout_seconds) {
        mtx_lock(&emergency->mutex);
        emergency->status = TIMEOUT;
        mtx_unlock(&emergency->mutex);
        
        snprintf(log_msg, sizeof(log_msg), "Emergency %d timed out (%.0f seconds elapsed)", 
                 emergency->id, difftime(now, emergency->time));
        write_log("SERVER", "EMERGENCY_STATUS", log_msg);
        
        free(data);
        return 0;
    }
    
    // Try to assign rescuers
    if (!assign_rescuers_to_emergency(emergency)) {
        mtx_lock(&emergency->mutex);
        emergency->status = TIMEOUT;
        mtx_unlock(&emergency->mutex);
        
        snprintf(log_msg, sizeof(log_msg), "Emergency %d timeout: insufficient resources", emergency->id);
        write_log("SERVER", "EMERGENCY_STATUS", log_msg);
        
        free(data);
        return 0;
    }
    
    // Simulate emergency response
    simulate_emergency_response(emergency);
    
    snprintf(log_msg, sizeof(log_msg), "Worker %d completed emergency %d", worker_id, emergency->id);
    write_log("SERVER", "WORKER_THREAD", log_msg);
    
    free(data);
    return 0;
}

int dispatcher_thread_func(void *arg) {
    write_log("SERVER", "DISPATCHER", "Dispatcher thread started");
    
    while (!terminate_flag) {
        emergency_t *emergency = NULL;
        int found = 0;
        
        // Check high priority first, then medium, then low
        for (int priority = 2; priority >= 0 && !found; priority--) {
            mtx_lock(&priority_queues[priority].mutex);
            
            if (priority_queues[priority].head != NULL) {
                emergency = dequeue_emergency(&priority_queues[priority]);
                found = 1;
            }
            
            mtx_unlock(&priority_queues[priority].mutex);
        }
        
        if (emergency != NULL) {
            // Create worker thread for this emergency
            worker_thread_data_t *data = malloc(sizeof(worker_thread_data_t));
            if (data == NULL) {
                write_log("SERVER", "MEMORY", "Failed to allocate worker thread data");
                continue;
            }
            
            data->emergency = emergency;
            data->worker_id = ++worker_count;
            
            thrd_t worker_thread;
            if (thrd_create(&worker_thread, worker_thread_func, data) != thrd_success) {
                write_log("SERVER", "THREAD", "Failed to create worker thread");
                free(data);
            } else {
                thrd_detach(worker_thread); // Let worker threads run independently
                
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Created worker %d for emergency %d", 
                         data->worker_id, emergency->id);
                write_log("SERVER", "DISPATCHER", log_msg);
            }
        } else {
            // No emergencies available, wait with timeout
            struct timespec sleep_time = {0, 500000000}; // 500ms
            thrd_sleep(&sleep_time, NULL);
        }
    }
    
    write_log("SERVER", "DISPATCHER", "Dispatcher thread terminated");
    return 0;
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    struct mq_attr queue_attr;
    
    printf("Emergency Management Server starting...\n");
    
    // Initialize logging
    init_logging();
    write_log("SERVER", "STARTUP", "Emergency Management Server started");
    
    // Parse configuration files
    parse_env("env.conf");
    parse_rescuers("rescuers.conf");
    parse_emergency_types("emergency_types.conf");
    
    // Initialize emergency ID mutex
    if (mtx_init(&emergency_id_mutex, mtx_plain) != thrd_success) {
        write_log("SERVER", "MUTEX", "Failed to initialize emergency ID mutex");
        exit(EXIT_FAILURE);
    }
    
    // Initialize priority queues
    for (int i = 0; i < 3; i++) {
        init_emergency_queue(&priority_queues[i]);
    }
    
    // Setup signal handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        write_log("SERVER", "SIGNAL", "Failed to setup SIGINT handler");
        perror("sigaction");
        cleanup_server();
        exit(EXIT_FAILURE);
    }
    
    // Create and configure message queue
    queue_attr.mq_flags = 0;
    queue_attr.mq_maxmsg = 10;
    queue_attr.mq_msgsize = sizeof(emergency_request_t);
    queue_attr.mq_curmsgs = 0;
    
    // Remove existing queue if any
    mq_unlink(env_config.queue_name);
    
    // FIXED: Create with both read and write permissions initially
    message_queue = mq_open(env_config.queue_name, O_CREAT | O_RDWR, 0666, &queue_attr);
    if (message_queue == -1) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to create message queue: %s", env_config.queue_name);
        write_log("SERVER", "MESSAGE_QUEUE", log_msg);
        perror("Failed to create message queue");
        cleanup_server();
        exit(EXIT_FAILURE);
    }
    
    // Close and reopen as read-only for receiving
    mq_close(message_queue);
    message_queue = mq_open(env_config.queue_name, O_RDONLY | O_NONBLOCK);
    if (message_queue == -1) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Failed to reopen message queue for reading: %s", env_config.queue_name);
        write_log("SERVER", "MESSAGE_QUEUE", log_msg);
        perror("Failed to reopen message queue");
        cleanup_server();
        exit(EXIT_FAILURE);
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Message queue created: %s", env_config.queue_name);
    write_log("SERVER", "MESSAGE_QUEUE", log_msg);
    
    // Start dispatcher thread
    if (thrd_create(&dispatcher_thread, dispatcher_thread_func, NULL) != thrd_success) {
        write_log("SERVER", "THREAD", "Failed to create dispatcher thread");
        cleanup_server();
        exit(EXIT_FAILURE);
    }
    
    write_log("SERVER", "STARTUP", "Server initialization completed");
    printf("Server ready. Listening for emergency requests...\n");
    printf("Press Ctrl+C to shutdown.\n");
    
    // FIXED: Main message receiving loop with timeout
    while (!terminate_flag) {
        emergency_request_t request;
        struct timespec timeout;
        
        // Set timeout to 1 second
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;
        
        // Receive message from queue with timeout
        ssize_t msg_size = mq_timedreceive(message_queue, (char*)&request, sizeof(request), NULL, &timeout);
        
        if (msg_size == -1) {
            if (errno == ETIMEDOUT) {
                // Timeout occurred, check terminate_flag and continue
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No messages available, sleep briefly and continue
                usleep(100000); // 100ms
                continue;
            } else if (errno == EINTR && terminate_flag) {
                break; // Interrupted by signal
            } else {
                write_log("SERVER", "MESSAGE_QUEUE", "Error receiving message");
                perror("mq_timedreceive");
                continue;
            }
        }
        
        if (msg_size != sizeof(request)) {
            write_log("SERVER", "MESSAGE_QUEUE", "Received message with invalid size");
            continue;
        }
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Received emergency request: %s at (%d,%d) timestamp=%ld", 
                 request.emergency_name, request.x, request.y, (long)request.timestamp);
        write_log("SERVER", "MESSAGE_QUEUE", log_msg);
        
        // Create emergency from request
        emergency_t *emergency = create_emergency_from_request(&request);
        if (emergency == NULL) {
            write_log("SERVER", "VALIDATION", "Invalid emergency request discarded");
            continue;
        }
        
        // Add to appropriate priority queue
        int priority = emergency->type.priority;
        enqueue_emergency(&priority_queues[priority], emergency);
        
        snprintf(log_msg, sizeof(log_msg), 
                 "Emergency %d queued with priority %d", emergency->id, priority);
        write_log("SERVER", "EMERGENCY_STATUS", log_msg);
    }
    
    write_log("SERVER", "SHUTDOWN", "Main loop terminated");
    cleanup_server();
    printf("Server shutdown completed.\n");
    return 0;
}