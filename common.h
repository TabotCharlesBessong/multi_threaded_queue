#ifndef COMMON_H
#define COMMON_H

#include <time.h>    // For time_t
#include <pthread.h> // For threading primitives
#include <signal.h>  // For signal handling

// =============================================================================
// CONSTANTS
// =============================================================================

#define EMERGENCY_NAME_LENGTH 64

// =============================================================================
// ENUMS
// =============================================================================

// Status for a rescuer unit (digital twin)
typedef enum
{
  IDLE,
  EN_ROUTE_TO_SCENE,
  ON_SCENE,
  RETURNING_TO_BASE
} rescuer_status_t;

// Status for an emergency instance
typedef enum
{
  WAITING,
  ASSIGNED,
  IN_PROGRESS,
  PAUSED, // For advanced preemption feature
  COMPLETED,
  CANCELED,
  TIMEOUT
} emergency_status_t;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// Represents a type of rescuer (e.g., "Pompieri")
typedef struct
{
  char *rescuer_type_name;
  int speed;
  int base_x; // Base X coordinate
  int base_y; // Base Y coordinate
} rescuer_type_t;

// Represents an individual rescuer unit (a "digital twin")
typedef struct
{
  int id;
  int current_x;
  int current_y;
  rescuer_type_t *rescuer_type; // Pointer to the type of this rescuer
  rescuer_status_t status;
} rescuer_digital_twin_t;

// Represents a request for a certain number of a specific rescuer type
// for a given emergency type
typedef struct
{
  rescuer_type_t *type;
  int required_count;
  int time_to_manage; // Time in seconds the unit will be busy on scene
} rescuer_request_t;

// Represents a type of emergency (e.g., "Incendio")
typedef struct
{
  char *emergency_desc;
  short priority;
  rescuer_request_t *rescuers; // Dynamically allocated array of rescuer requests
  int rescuers_req_number;     // Number of elements in the 'rescuers' array
} emergency_type_t;

// Represents a raw emergency request received from the message queue
typedef struct
{
  char emergency_name[EMERGENCY_NAME_LENGTH];
  int x;
  int y;
  time_t timestamp;
} emergency_request_t;

// Represents an active, tracked instance of an emergency
typedef struct
{
  long id; // Unique ID for this emergency instance
  emergency_type_t *type;
  emergency_status_t status;
  int x;
  int y;
  time_t time;                          // Timestamp when the emergency was registered
  int rescuer_count;                    // Total number of rescuers assigned
  rescuer_digital_twin_t **rescuers_dt; // Array of pointers to assigned rescuers
} emergency_t;

// =============================================================================
// FUNCTION PROTOTYPES (for parsers)
// =============================================================================

// Parser function prototypes
int parse_rescuers(const char* filename, rescuer_type_t** rescuers, int* rescuer_count);
int parse_emergency_types(const char* filename, emergency_type_t** emergency_types, int* emergency_type_count, rescuer_type_t* rescuers, int rescuer_count);
int parse_env(const char* filename, char* queue_name, int* max_emergencies, char* log_level, int* base_timeout);

// Utility functions
rescuer_type_t* find_rescuer_type(const char* name, rescuer_type_t* rescuers, int rescuer_count);
void free_rescuer_types(rescuer_type_t* rescuers, int count);
void free_emergency_types(emergency_type_t* emergency_types, int count);

// =============================================================================
// THREAD-SAFE QUEUE STRUCTURES
// =============================================================================

// Node for the priority queue
typedef struct queue_node {
    emergency_t* emergency;
    struct queue_node* next;
} queue_node_t;

// Thread-safe priority queue
typedef struct {
    queue_node_t* head;
    pthread_mutex_t mutex;
    int count;
} priority_queue_t;

// =============================================================================
// GLOBAL STATE STRUCTURE
// =============================================================================

typedef struct {
    rescuer_type_t* rescuers;
    int rescuer_count;
    emergency_type_t* emergency_types;
    int emergency_type_count;
    char queue_name[64];
    int max_emergencies;
    char log_level[16];
    int base_timeout;
    
    // Priority queues
    priority_queue_t high_priority_q;
    priority_queue_t medium_priority_q;
    priority_queue_t low_priority_q;
    
    // Global rescuer pool
    rescuer_digital_twin_t* rescuer_pool;
    int rescuer_pool_size;
    pthread_mutex_t rescuer_pool_mutex;
    
    // Shutdown flag
    volatile sig_atomic_t shutdown_requested;
} server_state_t;

// =============================================================================
// THREAD FUNCTION PROTOTYPES
// =============================================================================

void* listener_thread(void* arg);
void* dispatcher_thread(void* arg);
void* worker_thread(void* arg);

// =============================================================================
// QUEUE OPERATIONS
// =============================================================================

void init_priority_queue(priority_queue_t* queue);
void enqueue_emergency(priority_queue_t* queue, emergency_t* emergency);
emergency_t* dequeue_emergency(priority_queue_t* queue);
void cleanup_priority_queue(priority_queue_t* queue);

// =============================================================================
// SERVER OPERATIONS
// =============================================================================

int initialize_server(server_state_t* state);
void cleanup_server(server_state_t* state);
int create_rescuer_pool(server_state_t* state);

#endif // COMMON_H