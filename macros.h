#ifndef MACROS_H
#define MACROS_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/*
 * MACROS FOR SYSTEM CALL ERROR HANDLING
 *
 * These macros provide a centralized and consistent way to handle errors
 * from system calls, as required by the project specifications.
 */

// Define the standard value for a failed system call returning an integer
#define SCALL_ERROR -1

/**
 * @brief Handles errors for system calls that return -1 on failure.
 * @param r The variable to store the result of the system call.
 * @param c The system call to execute.
 * @param e An error message string to be passed to perror().
 *
 * If the system call `c` fails (returns -1), it prints the error message `e`
 * along with the system error description and exits the program with failure.
 */
#define SCALL(r, c, e) do { \
    if (((r) = (c)) == SCALL_ERROR) { \
        perror(e); \
        exit(EXIT_FAILURE); \
    } \
} while(0)


/**
 * @brief Handles errors for functions/system calls that return NULL on failure.
 * @param r The pointer variable to store the result of the call.
 * @param c The function call to execute.
 * @param e An error message string to be passed to perror().
 *
 * If the call `c` fails (returns NULL), it prints the error message `e`
 * along with the system error description and exits the program with failure.
 */
#define SNCALL(r, c, e) do { \
    if (((r) = (c)) == NULL) { \
        perror(e); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

#endif // MACROS_H