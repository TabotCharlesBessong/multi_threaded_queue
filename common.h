#ifndef COMMON_H
#define COMMON_H

#include <time.h>    // For time_t
#include <pthread.h> // For threading primitives

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

// We will add function prototypes here in the next step
// e.g., void parse_rescuers(const char* filename);