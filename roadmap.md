Of course. I have thoroughly analyzed the new documents. They provide critical, specific instructions that significantly clarify the project's requirements and even suggest a robust architectural pattern. The "Guide for the Project" is a set of explicit rules, while the "Example Report" gives us a successful template for the system's design.

Based on all the information we now have (the original 7-page PDF and these two new documents), here is a comprehensive updated project plan, including a refined architecture, a clear roadmap, and key design decisions.

---

### Updated Understanding and Executive Summary

The new documents confirm that this is a classic **client-server, producer-consumer system**. The "Guide" imposes strict coding standards (C11, specific macros, graceful shutdown), while the "Example Report" strongly suggests a specific and effective server architecture: a **Dispatcher-Worker thread model with multiple priority queues**.

Our primary goal is to build a system that is not only correct but also robust, maintainable, and compliant with all these new specific requirements.

### The Updated Design & Architecture

We will structure the project around the highly recommended model described in the example report. This design is robust and handles prioritization elegantly.

**1. The `client` (Producer):**
*   A simple, standalone executable (`producer` in the Makefile).
*   Its sole purpose is to read emergency data (from command line or file), construct an `emergency_request_t` message, and send it to the POSIX message queue.
*   It will be built using the provided code style and error-handling macros.

**2. The `server` (Consumer):**
This is the core of the system and will be composed of several concurrent components:

*   **Main Thread:**
    *   Initializes everything: parses config files, sets up the logger, creates the global rescuer pool, and initializes the three priority queues.
    *   **Crucially, it will set up a signal handler for `SIGINT` (Ctrl+C)** as demonstrated in the guide. This will allow for a graceful shutdown.
    *   Spawns the `Listener Thread` and the `Dispatcher Thread`.
    *   Enters a simple loop, waiting for a global `volatile sig_atomic_t` flag to be set by the signal handler.
    *   Once the flag is set, it will coordinate a clean shutdown and perform final cleanup.

*   **Listener Thread:**
    *   A single, dedicated thread that runs in an infinite loop, blocking on `mq_receive()`.
    *   When it receives a message, it validates the request.
    *   Based on the emergency's priority (read from the loaded `emergency_types` data), it places the new `emergency_t` object into one of **three separate, thread-safe queues**: `high_priority_q`, `medium_priority_q`, `low_priority_q`.
    *   Each queue will be a simple linked list protected by its own mutex.

*   **Dispatcher Thread:**
    *   This is the system's "brain." It runs in an infinite loop, acting as a scheduler.
    *   It checks the queues in a strict order:
        1.  Tries to lock and check the `high_priority_q`.
        2.  If empty, it checks the `medium_priority_q`.
        3.  If empty, it checks the `low_priority_q`.
    *   If it finds an emergency in a queue, it dequeues it and **spawns a new `Worker Thread`** to handle that specific emergency.
    *   If all queues are empty, it will `sleep` for a short interval to prevent busy-waiting, as suggested in the report.

*   **Worker Threads (Dynamically Created):**
    *   A new worker thread is created for **each individual emergency**.
    *   Its lifecycle is simple:
        1.  Receive the `emergency_t` data from the Dispatcher.
        2.  Lock the global rescuer list/pool to find and allocate the necessary `IDLE` rescuers.
        3.  Perform the time-to-arrival calculation (Manhattan distance). If it fails the deadline, set status to `TIMEOUT`, free the rescuers, log the event, and exit.
        4.  If successful, update rescuer and emergency statuses. Unlock the global list.
        5.  Simulate travel time (e.g., `sleep()`).
        6.  Simulate on-scene work time (`sleep()`).
        7.  Simulate return-to-base time (`sleep()`).
        8.  Once finished, lock the global rescuer list again to set the rescuers' status back to `IDLE` and their location to base.
        9.  The thread's work is done, and it terminates (`pthread_exit`).

### Key Changes & Clarifications from New Documents

1.  **Thread Library (`pthread.h`):** The guide mentions "Use thread.h not pthread". This is almost certainly a typo. The standard POSIX threads library in C is `pthread.h`. The provided Makefile template confirms this by including the `-lpthread` linker flag. **We will proceed using `pthread.h`**, as it is the correct and intended standard.

2.  **Mandatory Error-Handling Macros:** We must use the exact style of macros shown in the guide (e.g., `SCALL(r, c, e)`). We will create a `macros.h` file for these and use them for every system call.

3.  **Graceful Shutdown:** The server must not be terminated abruptly. The provided logger example shows a sophisticated `SIGINT` handler. We will implement this to ensure logs are flushed and resources are cleaned up properly on Ctrl+C.

4.  **Makefile:** We will adapt the provided Makefile template. The `consumer` will be our `server` executable, and the `producer` will be our `client`.

### Updated Project Roadmap & Timeline

This roadmap is more detailed and incorporates the new requirements.

**Step 1: Project Skeleton & Core Components (Day 1)**
*   Create the file structure: `server.c`, `client.c`, `parsers.c`, `logger.c`, `common.h`, `macros.h`.
*   Implement the `Makefile` based on the template.
*   In `common.h`, define all `struct`s and `enum`s from the original PDF.
*   In `macros.h`, implement the mandatory `SCALL`, `SNCALL`, etc., error-handling macros.

**Step 2: Configuration Parsers & Initialization (Day 1-2)**
*   Implement the functions in `parsers.c` to read `rescuers.conf`, `emergency_types.conf`, and `env.conf`.
*   **Requirement:** All file operations (`fopen`, `fgets`, etc.) within the parsers must be protected by the new macros.
*   The `server`'s `main` function will call these parsers to load all data into global structures.
*   **Milestone:** The server can be compiled and run. It will parse all files, print the loaded data to the console for verification, and then exit cleanly.

**Step 3: Client, Message Queue, and Listener (Day 3)**
*   Implement the `client.c` program to send a hardcoded message to the POSIX queue.
*   In `server.c`, implement the `Listener Thread`. Its only job for now is to receive messages from the queue and log them to the console.
*   **Milestone:** We can run `./client` and see the message appear on the `server`'s console, proving the IPC channel works.

**Step 4: The Dispatcher & Worker Core Logic (Day 4-5)**
*   Implement the **three priority queues** (as thread-safe linked lists).
*   Modify the `Listener Thread` to place incoming emergencies into the correct queue.
*   Implement the `Dispatcher Thread` logic to scan the queues.
*   Implement the logic for a **single `Worker Thread`**. This includes:
    *   Locking the shared rescuer data pool (using a mutex).
    *   Resource allocation logic.
    *   Manhattan distance and time-to-arrival calculation.
    *   State transitions for the emergency and rescuers.
*   **Milestone:** The server can process one emergency at a time, from reception to completion, with all state changes logged correctly. Test with Valgrind's Memcheck and Helgrind.

**Step 5: Full Concurrency & Graceful Shutdown (Day 6)**
*   Enable the Dispatcher to spawn a new worker thread for *every* emergency it finds. This is the final step to achieve full concurrency.
*   Implement the `SIGINT` signal handler and the main server loop's shutdown logic.
*   **Milestone:** The system can handle multiple, concurrent emergencies. It can be shut down cleanly with Ctrl+C, ensuring all logs are saved. Conduct stress tests by sending many messages from the client.

**Step 6: Finalization & Report (Day 7)**
*   Conduct thorough testing, focusing on edge cases and resource cleanup. Run a final check with Valgrind (both Memcheck and Helgrind) to ensure there are no memory leaks or race conditions.
*   Write the final report. We will use the provided "Example Report" as a structural guide, detailing our implementation of each component (Parsing, Client, Server architecture, `common.h`, Makefile).

This updated plan is concrete, addresses all known requirements, and follows the best practices and architectural patterns suggested by the new documents. We are well-prepared to build the project successfully.