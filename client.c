#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "macros.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <emergency_name> <x> <y> <priority>\n", argv[0]);
        fprintf(stderr, "Example: %s Incendio 150 200 3\n", argv[0]);
        return 1;
    }

    printf("Emergency Client starting...\n");

    // Parse command-line arguments
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x, y, priority;
    
    strncpy(emergency_name, argv[1], EMERGENCY_NAME_LENGTH - 1);
    emergency_name[EMERGENCY_NAME_LENGTH - 1] = '\0';
    
    x = atoi(argv[2]);
    y = atoi(argv[3]);
    priority = atoi(argv[4]);

    // Validate coordinates and priority
    if (x < 0 || y < 0 || priority < 1 || priority > 3) {
        fprintf(stderr, "Invalid parameters: x and y must be >= 0, priority must be 1-3\n");
        return 1;
    }

    printf("Emergency request details:\n");
    printf("  Emergency: %s\n", emergency_name);
    printf("  Location: (%d, %d)\n", x, y);
    printf("  Priority: %d\n", priority);
    printf("  Timestamp: %lld\n", (long long)time(NULL));

    printf("\nNote: This is a test client. In a full implementation,\n");
    printf("it would send this request to the server via message queue.\n");

    printf("Emergency Client finished.\n");
    return 0;
}