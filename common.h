#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <mqueue.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <threads.h>
#include <stdatomic.h>

// Constants
#define EMERGENCY_NAME_LENGTH 64
#define MAX_MSG_SIZE 512
#define MAX_RESCUERS 100
#define MAX_EMERGENCY_TYPES 50
#define MAX_RESCUER_TYPES 20
#define LOG_FILENAME "emergency_system.log"

// Error handling macros
#define SCALL_ERROR -1
#define SCALL(r, c, e) do { \
    if((r = c) == SCALL_ERROR) { \
        perror(e); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#define SNCALL(r, c, e) do { \
    if((r = c) == NULL) { \
        perror(e); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#define ERR_OPEN(file, mode) do { \
    if((file = fopen(#file, mode)) == NULL) { \
        perror("Error opening " #file); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#define ERR_MALLOC(ptr, size) do { \
    if((ptr = malloc(size)) == NULL) { \
        perror("Error allocating memory"); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

// Rescuer status enumeration
typedef enum {
    IDLE,
    EN_ROUTE_TO_SCENE,
    ON_SCENE,
    RETURNING_TO_BASE
} rescuer_status_t;

// Emergency status enumeration
typedef enum {
    WAITING,
    ASSIGNED,
    IN_PROGRESS,
    PAUSED,
    COMPLETED,
    CANCELED,
    TIMEOUT
} emergency_status_t;

// Rescuer type structure
typedef struct {
    char *rescuer_type_name;
    int speed;
    int x;
    int y;
} rescuer_type_t;

// Digital twin structure for rescuers
typedef struct {
    int id;
    int x;
    int y;
    rescuer_type_t *rescuer;
    rescuer_status_t status;
    mtx_t mutex;
} rescuer_digital_twin_t;

// Rescuer request structure
typedef struct {
    rescuer_type_t *type;
    int required_count;
    int time_to_manage;
} rescuer_request_t;

// Emergency type structure
typedef struct {
    short priority;
    char *emergency_desc;
    rescuer_request_t *rescuers;
    int rescuers_req_number;
} emergency_type_t;

// Emergency request structure (for message queue)
typedef struct {
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x;
    int y;
    time_t timestamp;
} emergency_request_t;

// Emergency structure
typedef struct {
    emergency_type_t type;
    emergency_status_t status;
    int x;
    int y;
    time_t time;
    int rescuer_count;
    rescuer_digital_twin_t **rescuers_dt;
    mtx_t mutex;
    int id;
} emergency_t;

// Environment configuration
typedef struct {
    char queue_name[64];
    int height;
    int width;
} environment_config_t;

// Global variables (declared as extern)
extern rescuer_type_t rescuer_types[MAX_RESCUER_TYPES];
extern int rescuer_types_count;
extern rescuer_digital_twin_t rescuers[MAX_RESCUERS];
extern int rescuers_count;
extern emergency_type_t emergency_types[MAX_EMERGENCY_TYPES];
extern int emergency_types_count;
extern environment_config_t env_config;
extern FILE *log_file;
extern mtx_t log_mutex;

// Function declarations
void parse_rescuers(const char *filename);
void parse_emergency_types(const char *filename);
void parse_env(const char *filename);

// Logging functions
void write_log(const char *id, const char *event, const char *message);
void init_logging(void);
void cleanup_logging(void);

// Utility functions
int calculate_manhattan_distance(int x1, int y1, int x2, int y2);
double calculate_travel_time(int distance, int speed);
rescuer_type_t* find_rescuer_type(const char *name);
emergency_type_t* find_emergency_type(const char *name);

// Thread-safe queue for emergencies
typedef struct emergency_queue_node {
    emergency_t *emergency;
    struct emergency_queue_node *next;
} emergency_queue_node_t;

typedef struct {
    emergency_queue_node_t *head;
    emergency_queue_node_t *tail;
    mtx_t mutex;
    cnd_t condition;
    int count;
} emergency_queue_t;

extern emergency_queue_t priority_queues[3]; // 0=low, 1=medium, 2=high

// Queue operations
void init_emergency_queue(emergency_queue_t *queue);
void enqueue_emergency(emergency_queue_t *queue, emergency_t *emergency);
emergency_t* dequeue_emergency(emergency_queue_t *queue);
void cleanup_emergency_queue(emergency_queue_t *queue);

#endif // COMMON_H