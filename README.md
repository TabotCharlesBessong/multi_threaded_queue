# Emergency Management System

A multithreaded C11 emergency management system using POSIX message queues for distributed emergency handling.

## Project Overview

This system simulates an emergency management service that:
- Receives emergency requests through a POSIX message queue
- Manages different types of rescuers (firefighters, ambulances, police)
- Assigns rescuers to emergencies based on priority and availability
- Uses digital twins to simulate rescuer movement and status
- Handles concurrent emergency processing with proper synchronization

## Architecture

### Core Components

1. **Server** (`server.c`): Main emergency management system
   - Message queue listener
   - Emergency dispatcher thread
   - Worker threads for emergency handling
   - Rescuer assignment and simulation

2. **Client** (`client.c`): Emergency request generator
   - Single emergency requests
   - Batch processing from files
   - Configurable delays

3. **Parsers**: Configuration file processors
   - `parse_rescuers.c`: Rescuer types and instances
   - `parse_emergency_types.c`: Emergency definitions
   - `parse_env.c`: Environment configuration

4. **Utilities** (`utils.c`): Common functionality
   - Logging system
   - Queue operations
   - Distance calculations

### Data Structures

- **Digital Twins**: Simulate rescuer units with status and location
- **Priority Queues**: Three queues for different emergency priorities
- **Thread-Safe Operations**: Mutex protection for all shared resources

## Building and Running

### Prerequisites
- Linux Ubuntu 24.04 (or compatible)
- GCC with C11 support
- POSIX threads and message queues support

### Quick Start

1. **Build everything:**
```bash
make setup_configs  # Create example configuration files
make               # Build server and client
```

2. **Start the server** (Terminal 1):
```bash
./server
```

3. **Send emergencies** (Terminal 2):
```bash
# Single emergency
./client Incendio 120 180 2

# Multiple emergencies from file
./client -f emergencies.txt
```

### Makefile Targets

- `make` - Build both server and client
- `make setup_configs` - Create example configuration files
- `make test` - Run automated test scenario
- `make clean` - Remove build artifacts
- `make help` - Show all available targets

## Configuration Files

### rescuers.conf
Defines rescuer types and their properties:
```
[Pompieri][5][2][100;200]
[Ambulanza][3][4][150;250]
[Polizia][4][3][50;100]
```
Format: `[Name][Count][Speed][BaseX;BaseY]`

### emergency_types.conf
Defines emergency types and required resources:
```
[Incendio][2]Pompieri:2,8;Ambulanza:1,2;
[Allagamento][1]Pompieri:1,5;Ambulanza:1,3;
```
Format: `[Name][Priority]RescuerType:Count,TimeInSeconds;`

### env.conf
System environment configuration:
```
queue=emergenze123456
height=300
width=400
```

## System Features

### Concurrency Management
- **Thread-safe queues** with mutex protection
- **Worker thread pool** for emergency handling
- **Digital twin synchronization** for rescuer status
- **Deadlock prevention** through consistent lock ordering

### Priority System
- **High Priority (2)**: 10-second timeout
- **Medium Priority (1)**: 30-second timeout  
- **Low Priority (0)**: No timeout

### Emergency Lifecycle
1. **WAITING**: Received and validated
2. **ASSIGNED**: Rescuers allocated
3. **IN_PROGRESS**: Rescuers responding
4. **COMPLETED**: Emergency resolved
5. **TIMEOUT**: Failed to allocate resources in time

### Rescuer States
- **IDLE**: Available at base
- **EN_ROUTE_TO_SCENE**: Moving to emergency
- **ON_SCENE**: Handling emergency
- **RETURNING_TO_BASE**: Returning after completion

## Distance and Time Calculations

- **Manhattan Distance**: `|x1-x2| + |y1-y2|`
- **Travel Time**: `distance / speed` seconds
- **Response Simulation**: Realistic movement and intervention times

## Logging System

Comprehensive logging to `emergency_system.log`:
- Configuration parsing events
- Message queue operations
- Emergency status transitions
- Rescuer status changes
- Thread operations and assignments

Log format: `[Timestamp] [ID] [Event] Message`

## Error Handling

- **System call protection** with error macros
- **Resource validation** for all inputs
- **Memory allocation checks**
- **Thread synchronization error handling**
- **Graceful shutdown** with resource cleanup

## Client Usage

### Command Line Mode
```bash
./client <emergency_name> <x> <y> <delay_seconds>
```

### File Mode
```bash
./client -f <filename>
```

File format (one emergency per line):
```
Incendio 120 180 1
Allagamento 50 75 3
Sommossa 200 250 2
```

## Technical Implementation

### Thread Safety
- All shared data structures protected by mutexes
- Condition variables for queue signaling
- Atomic operations for simple counters
- No race conditions or deadlocks

### Memory Management
- Proper allocation and deallocation
- Resource cleanup on shutdown
- Memory leak prevention

### Signal Handling
- Graceful shutdown on SIGINT (Ctrl+C)
- Thread-safe signal processing
- Resource cleanup on termination

## Testing

### Automated Test
```bash
make test
```

This runs a complete scenario:
1. Creates configuration files
2. Starts server
3. Sends multiple emergencies
4. Demonstrates system operation
5. Clean shutdown

### Manual Testing

1. **Basic Operation**:
   - Start server, send single emergency
   - Verify logs and console output

2. **Priority Testing**:
   - Send emergencies with different priorities
   - Verify high-priority emergencies are processed first

3. **Resource Contention**:
   - Send more emergencies than available rescuers
   - Verify proper queuing and timeout handling

4. **File Processing**:
   - Test batch emergency processing
   - Verify delays and sequencing

## Project Structure

```
emergency-system/
├── common.h              # Shared definitions and structures
├── server.c             # Main server application
├── client.c             # Emergency request client
├── parse_rescuers.c     # Rescuer configuration parser
├── parse_emergency_types.c # Emergency type parser
├── parse_env.c          # Environment configuration parser
├── utils.c              # Utility functions and logging
├── Makefile             # Build configuration
├── README.md            # This documentation
├── rescuers.conf        # Rescuer configuration
├── emergency_types.conf # Emergency type definitions
├── env.conf            # Environment settings
└── emergencies.txt     # Example emergency batch file
```

## Compliance

- **C11 Standard**: Full compliance with C11 features
- **POSIX Compatibility**: Uses POSIX threads and message queues
- **Linux Ubuntu 24.04**: Tested and verified
- **Thread Safety**: All concurrent operations properly synchronized
- **Error Handling**: Comprehensive system call protection

## Future Enhancements

Potential extensions for advanced requirements:
- **Preemption**: Interrupt lower-priority emergencies
- **Aging**: Dynamic priority increases to prevent starvation
- **Load Balancing**: Optimize rescuer distribution
- **Geographic Optimization**: Smart rescuer selection by distance
- **Real-time Monitoring**: Web dashboard for system status

## Authors and License

Developed for Laboratory 2 course requirements.
Academic project implementing multithreaded emergency management system.