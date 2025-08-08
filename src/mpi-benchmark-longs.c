#include "../neighbour/mpi-neighbour.h"
#include "benchmarking.h"
#include <arpa/inet.h>
#include <math.h>
#include <mpi.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// TODO:
// -Adapt code to measure benchmarking stats on x86 hosts (DRAC)
// -Remove 50ms delay in main loop?

// Arguments (defaults)
int arg_nMax = -1, arg_depth = -1, arg_exchanges = 4, arg_debug = 0, arg_benchmark = 0, arg_use_fake_data = 0, flag_allGather = 0;
char* arg_local_logfile = "";

// Defines / tunables
#define EXCHANGED_HOST_DATA_COUNT 9                 // Number of values shared between BlueFields when gathering
#define PREVIOUS_VALUE_COUNT 6                      // Number of values we keep track of for load calculations
#define HOST_TRANSFER_COUNT 8                       // Number of longs received from host upon request
#define SERVER_PORT 56789                           // Used for host<->bf counter exchange. Must match port defined in server code.
#define HOST_LOGFILE_BASE_NAME "host_data.log"      // Where to write out the data retrieved from the remote hosts by BFs (if using "real" data to exchange) Will try to write
                                                    // to "example.log", then if it exists "example0.log", then "example1.log"... etc.
#define LOCAL_LOGFILE_BASE_NAME "local_data.csv"    // Where to write the local (BFs') log data (see above comment about HOST_LOGFILE_BASE_NAME)

// Functions:

/**
 *  @update_global_view
 *  
 *  Update a leader's global system view using newly received data [PLACEHOLDER]
 *
 *  @recBuffer:     Pointer to updated system view.
 *
 */
void update_global_view(long *recBuffer) {
    return;
}

/**
 *  @compute_host_load
 *  
 *  Fetch updated counter values from associated host, calculate updated load statistics and prepare them in passed sendBuffer
 *
 *  @sendBuffer:        Pointer to buffer in which to place host's updated state (this buffer will be exchanged with neighbourhood leader)
 *  @previousValues:    Pointer to previous values of certain counters received from remote host. Needed to calculate averages or load over time.
 *  @worldRank:         This BF's rank in MPI_COMM_WORLD (this value is shared along with remote host's actual stats)
 *  @sockfd:            Socket file descriptor which points to correct remote host
 *  @server_addr:       Pointer to sockaddr_in struct pointing to remote host (addr + port)
 *  @addr_len:          Size of address struct in bytes
 *
 */
void compute_host_load(long *sendBuffer, long *previousValues, int worldRank, int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len) {

    // Store world_rank as first number in returned data
    sendBuffer[0] = (long)worldRank;

    // Store data received from host temporarily
    long receivedData[HOST_TRANSFER_COUNT];

    // Message to indicate "All is well, send new data please"
    const char *data_request_msg = "x";

    // Send message
    sendto(sockfd, data_request_msg, strlen(data_request_msg), 0, (const struct sockaddr *)server_addr, addr_len);

    // Receive response
    ssize_t n = recvfrom(sockfd, receivedData, HOST_TRANSFER_COUNT * sizeof(long), 0, (struct sockaddr *)server_addr, &addr_len);

    if (n < 0) {
                    perror("recvfrom failed");
                    close(sockfd);
                    MPI_Finalize();
                    exit(-4);
    }

    // Array contents / mappings:

    // previousValues[0] = cpu_idle
    // previousValues[1] = working
    // previousValues[2] = io_wait
    // previousValues[3] = rcv
    // previousValues[4] = xmit
    // previousValues[5] = ctxt

    // receivedData[0] = cpu_idle;
    // receivedData[1] =
    // cpu_user+cpu_nice+cpu_system+cpu_irq+cpu_softirq+cpu_steal+cpu_guest+cpu_guest_nice;
    // // cpu time spent "working" receivedData[2] = cpu_iowait; receivedData[3] =
    // rcv; receivedData[4] = xmit; receivedData[5] = ctxt; receivedData[6] =
    // procs_running; receivedData[7] = procs_blocked;

    // sendBuffer[0] = worldRank
    // sendBuffer[1] = idle%
    // sendBuffer[2] = working%
    // sendBuffer[3] = io_wait%
    // sendBuffer[4] = rcv_delta
    // sendBuffer[5] = xmit_delta
    // sendBuffer[6] = ctxt_delta (context switches)
    // sendBuffer[7] = procs_running
    // sendBuffer[8] = procs_blocked

    // Compute new statistics which will be exchanged among nodes
    long elapsed_cycles = ((receivedData[0] + receivedData[1] + receivedData[2]) - (previousValues[0] + previousValues[1] + previousValues[2]));
    sendBuffer[1] = (long)round(100 * (double)(receivedData[0] - previousValues[0]) / elapsed_cycles); // idle
    sendBuffer[2] = (long)round(100 * (double)(receivedData[1] - previousValues[1]) / elapsed_cycles); // "working" (in user, kernel, whatever mode)
    sendBuffer[3] = (long)round(100 * (double)(receivedData[2] - previousValues[2]) / elapsed_cycles); // iowait

    sendBuffer[4] = receivedData[3] - previousValues[3];
    sendBuffer[5] = receivedData[4] - previousValues[4];
    sendBuffer[6] = receivedData[5] - previousValues[5];

    sendBuffer[7] = receivedData[6];
    sendBuffer[8] = receivedData[7];

    // Save newly fetched statistics as previousValues for next iteration
    previousValues[0] = receivedData[0];
    previousValues[1] = receivedData[1];
    previousValues[2] = receivedData[2];
    previousValues[3] = receivedData[3];
    previousValues[4] = receivedData[4];
    previousValues[5] = receivedData[5];

    return;
}

/**
 *  @print_buffer_char
 *  
 *  Helper to print array of 'char' type (super-duper unsafe)
 *
 *  @data:      Pointer to data to print
 *  @len:       Number of items to print
 *
 */
void print_buffer_char(char *data, int len) {
    for (int i = 0; i < len; i++) {
            printf("%c ", data[i]);
    }
    printf("\n");
    return;
}

/**
 *  @print_buffer_long
 *  
 *  Helper to print array of 'long' type (super-duper unsafe)
 *
 *  @data:      Pointer to data to print
 *  @len:       Number of items to print
 *
 */
void print_buffer_long(long *data, int len) {
    for (int i = 0; i < len; i++) {
            printf("%ld ", data[i]);
    }
    printf("\n");
    return;
}

/**
 *  @print_buffer_long_long
 *  
 *  Helper to print array of 'long long' type (super-duper unsafe)
 *
 *  @data:      Pointer to data to print
 *  @len:       Number of items to print
 *
 */
void print_buffer_long_long(long long *data, int len) {
    for (int i = 0; i < len; i++) {
            printf("%lld ", data[i]);
    }
    printf("\n");
    return;
}

/**
 *  @print_help
 *  
 *  Print the passed string along with the help message describing correct usage of this script and arguments.
 *
 *  @error_message:     Null-terminated error message to print before generic help.
 *
 */
void print_help(const char *error_message) {
    if (getenv("OMPI_COMM_WORLD_RANK") &&
                    atoi(getenv("OMPI_COMM_WORLD_RANK")) != 0) {
            return;
    }

    printf("%s", error_message);
    printf("Usage: mpirun -np <N> ./mpi-benchmark-testing [options]\n");
    printf("  -e or --exchanges <int>               Number of global exchanges (default: 4)\n");
    printf("  -n or --neighbourhood-size <int>      Maximum neighborhood size, must be >1 (default: 2). If n > number of processes, all gather will occur at a depth of 1 regardless of what d is, if n == number of processes, a single neighbourhood will be used with no all gather\n");
    printf("  -d or --depth <int>                   The Depth at which all gather will occur. Furthermore, setting this alone controls the neighbourhood size, defaulting to a value that accomodates n = 2 if the depth is too big.\n");
    printf("                                        NOTE: If both n and d are set, neighbourhood size of n will be used if possible, if neighbourhoods of n cause the depth value to change, depth will be reduced to enable all gather on the final layer.\n");
    printf("  -v or --verbose <int>                 Print debugging messages, varrying in granularity 1-10 (default: off)\n");
    printf("  -h or --help                          Show this help message\n");
    printf("  -b or --benchmark <int>               Number of \"items\" (longs) each BlueField should send per exchange.\n This option should be used for benchmarking data exchange speeds (meaningless information is exchanged).\n If it is omitted, the benchmark will exchange actual information retrieved from hosts.\n");

    return;
}

/**
 *  @arg_parse
 *  
 *  Parse arguments to set global params and handle errors. 
 *
 *  @argc:      Number of arguments (incld. executable's name as arg[0])
 *  @argv:      List of arguments
 *  @rank:      This BF's rank in MPI_COMM_WORLD
 *
 */
void arg_parse(int argc, char **argv, int rank) {

        for (int i = 1; i < argc; ++i) {

                if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                        print_help("");
                        MPI_Finalize();
                        exit(0);
                } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                        if (++i < argc) {
                                arg_debug = atoi(argv[i]);
                                if (arg_debug < 0 || arg_debug > 10) {
                                        char buffer[128];
                                        snprintf(buffer, sizeof(buffer), "Invalid value passed for argument \"-v\"/\"--verbose\": %s, value must be between 1 and 10\n\n",argv[i]);
                                        print_help(buffer);
                                        MPI_Finalize();
                                        exit(-1);
                                }
                                continue;
                        } else {
                                print_help("No value passed for \"-v\"/\"--verbose\"\n\n");
                                MPI_Finalize();
                                exit(-2);
                        }
                } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--logfile")) {
                        if (++i < argc) {
                                arg_local_logfile = argv[i];
                        } else {
                                print_help("No value passed for \"-l\"/\"--logfile\"\n\n");
                                MPI_Finalize();
                                exit(-2);
                        }
                
                } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--benchmark")) {
                        if (++i < argc) {
                                arg_benchmark = atoi(argv[i]);
                                if (!arg_benchmark) {
                                        char buffer[64];
                                        snprintf(buffer, sizeof(buffer), "Invalid value passed for argument \"-b\"/\"--benchmark\": %s\n\n", argv[i]);
                                        print_help(buffer);
                                        MPI_Finalize();
                                        exit(-1);
                                }
                                arg_use_fake_data = 1;
                                continue;
                        } else {
                                print_help("No value passed for \"-b\"/\"--benchmark\"\n\n");
                                MPI_Finalize();
                                exit(-2);
                        }
                } else if (!strcmp(argv[i], "-n") ||
                                                         !strcmp(argv[i], "--neighbourhood-size")) {
                        if (++i < argc) {
                                arg_nMax = atoi(argv[i]);
                                if (arg_nMax < 2) {
                                        char buffer[128];
                                        snprintf(buffer, sizeof(buffer),"Invalid value passed for argument \"-n\"/\"--neighbourhood-size\": %s\n\n", argv[i]);
                                        print_help(buffer);
                                        MPI_Finalize();
                                        exit(-1);
                                }
                                continue;
                        } else {
                                print_help("No value passed for \"-n\"/\"--neighbourhood-size\"\n\n");
                                MPI_Finalize();
                                exit(-2);
                        }
                } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--depth")) {
                        if (++i < argc) {
                                arg_depth = atoi(argv[i]);
                                if (!arg_depth) {
                                        char buffer[128];
                                        snprintf(buffer, sizeof(buffer), "Invalid value passed for argument \"-d\"/\"--depth\": %s\n\n", argv[i]);
                                        print_help(buffer);
                                        MPI_Finalize();
                                        exit(-1);
                                }
                                continue;
                        } else {
                                print_help("No value passed for \"-d\"/\"--depth\"\n\n");
                                MPI_Finalize();
                                exit(-2);
                        }
                } else if (!strcmp(argv[i], "-e") || !strcmp(argv[i], "--exchanges")) {
                        if (++i < argc) {
                                arg_exchanges = atoi(argv[i]);
                                if (!arg_exchanges) {
                                        char buffer[64];
                                        snprintf(buffer, sizeof(buffer), "Invalid value passed for argument \"-e\"/\"--exchanges\": %s\n\n", argv[i]);
                                        print_help(buffer);
                                        MPI_Finalize();
                                        exit(-1);
                                }
                                continue;
                        } else {
                                print_help("No value passed for \"-e\"/\"--exchanges\"\n\n");
                                MPI_Finalize();
                                exit(-2);
                        }
                } else {
                        char buffer[64];
                        snprintf(buffer, sizeof(buffer), "Unrecognized argument: \"%s\"\n\n", argv[i]);
                        print_help(buffer);
                        MPI_Finalize();
                        exit(-3);
                }
        }

        if (arg_debug == 1 && rank == 0) {
                printf("Arguments: arg_benchmark: %d\targ_exchanges: %d\targ_debug: %d\targ_depth: %d\targ_nMax: %d\targ_local_logfile: %s\n", arg_benchmark, arg_exchanges, arg_debug, arg_depth, arg_nMax, arg_local_logfile);
        }

        return;
}

int main(int argc, char **argv) {
    int rank, size;
    srand(time(NULL)); // Seed RNG

    // Initialize
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Argument parsing
    arg_parse(argc, argv, rank);

    // Who am I, and who is my parent (x86 host)?
    char hostname[256];
    char parentHost[256];

    gethostname(hostname, sizeof(hostname));
    printf("RANK: %d, HOSTNAME: %s\n", rank, hostname);
    memcpy(parentHost, hostname, 4);
    memcpy(parentHost + 4, hostname + 8, 3);
    parentHost[8] = '\0'; // getaddrinfo needs null-terminated

    // Are we simply benchmarking data exchange with meaningless data, or wanting
    // to retrieve and exchange information from hosts?
    long *previousValues = NULL;
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    if (!arg_use_fake_data) {
        arg_benchmark = EXCHANGED_HOST_DATA_COUNT; // Number of data points to exchange between BlueFields about their hosts

        // Used to store previously read host counter values (for computing load over time)
        previousValues = malloc(PREVIOUS_VALUE_COUNT * sizeof(long));
        memset(previousValues, 0, PREVIOUS_VALUE_COUNT * sizeof(long));

        // Host-BF communication client-side setup:

        // Create UDP socket
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Resolve hostname to IP address
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;      // IPv4
        hints.ai_socktype = SOCK_DGRAM; // UDP

        int status = getaddrinfo(parentHost, NULL, &hints, &res); // Actually resolve
        if (status != 0) {
            fprintf(stderr, "\n\ngetaddrinfo error: %s\n\n", gai_strerror(status));
            close(sockfd);
            MPI_Finalize();
            exit(-3); // Should there be more handling here? (i.e. tell other BFs and let them continue without us?)
        }

        // Set up server address struct using resolved IP
        struct sockaddr_in *resolved = (struct sockaddr_in *)res->ai_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        server_addr.sin_addr = resolved->sin_addr;

        freeaddrinfo(res);
    }

    // If we're not using real data, EXCHANGED_HOST_DATA_COUNT is passed for no reason
    initialize_benchmarking(size, rank, arg_exchanges, EXCHANGED_HOST_DATA_COUNT); 

    // Neighborhood stuff
    MPI_Comm *n;
    NeighbourhoodShape *ns = malloc(sizeof(NeighbourhoodShape));

    createCommsArray(&n, &arg_depth, &arg_nMax, &flag_allGather, arg_debug);

    createNeighbourhoodView(&n, ns, arg_depth, arg_nMax, arg_benchmark, flag_allGather);

    if (1) {//if (arg_debug >= 4) {
        printNeighbourhoodView(&n, ns, rank, arg_depth, arg_nMax, flag_allGather);
    }

    long *recBuffer, *sendBuffer;
    long sendBufferSize =  arg_benchmark * sizeof(long);
    sendBuffer = malloc(sendBufferSize);
    memset(sendBuffer, 0, arg_benchmark * sizeof(long));

    // Main loop (update values, exchange them)
    for (int i = 0; i < arg_exchanges; i++) {

        // Track beginning of fetching host counters (or generating fake data)
        benchmark_log_time(i);

        if (arg_use_fake_data) {
            for (int i = 0; i < arg_benchmark; i++) { // Fill buffer with fake data
                long data = (long)((rank + 1) * (i + 1));
                sendBuffer[i] = data;
            }
        } else {
            compute_host_load(sendBuffer, previousValues, rank, sockfd, &server_addr, addr_len);
        }

        // Track end of fetching host counters (or generating fake data) / beginning of data exchange
        benchmark_log_time(i);

        // Exchange data with other BFs
        gatherCommData(&n, ns, &recBuffer, sendBuffer, arg_nMax, arg_depth, arg_benchmark, flag_allGather, arg_debug);

        // Track end of data exchange / beginning of fetching benchmark-related counters
        benchmark_log_time(i);

         // Look at each BF's network usage
        fetch_benchmarking_counters(i);

        // 50ms. This probably shouldn't exist in the final release.
        usleep(50000); 

        // Leaders update global system views (don't do this if we're only benchmarking the data exchange speeds)
        if (rank == 0 && !arg_use_fake_data) {
            log_remote_counters(recBuffer, EXCHANGED_HOST_DATA_COUNT); // Log the counter data received from all remote hosts for benchmarking purposes
            update_global_view(recBuffer);
        }

        // Display global system view (debugging)
        if (rank == 0 && arg_debug >= 2 && !arg_use_fake_data) {
            printf("========================Received host information========================\n\n");
            for (int rank_n = 0; rank_n < size; rank_n++) {
                printf("Rank %ld:\n", recBuffer[rank_n * EXCHANGED_HOST_DATA_COUNT]);
                for (int index = 0; index < EXCHANGED_HOST_DATA_COUNT; index++) {
                    printf("%ld ", recBuffer[rank_n * EXCHANGED_HOST_DATA_COUNT + index]);
                }
                printf("\n\n");
            }
        } else if (arg_use_fake_data && arg_debug >= 1) {
            if (arg_debug >= 2) {
                printf("arg_benchmark %d. arg_nMax %d, arg_depth %d\n", arg_benchmark, arg_nMax, arg_depth);
            }

            int numElements = arg_benchmark * (pow(abs(arg_nMax), abs(arg_depth)));
            printData(MPI_COMM_WORLD, 0, recBuffer, arg_benchmark, 100000, numElements, arg_debug);
        }

        // Get a temp rank to identify who should free recBuffer
        int tempRank;
        if (n[0] != MPI_COMM_NULL) {
            MPI_Comm_rank(n[0], &tempRank);
        }

        // Only free recBuffer if it was allocated, in the all gather case this only happens when the comm exists in the last layer of n
        if (flag_allGather == 1 && n[arg_depth - 1] != MPI_COMM_NULL) {
            int tempSize;
            MPI_Comm_size(n[arg_depth - 1], &tempSize);
            free(recBuffer);
        } else if (tempRank == 0) {
            // Frees the recBuffer for every comm that is ever a leader
            free(recBuffer);
        }

    } // end of main loop

    // Send shutdown message to server process on remote host
    if (!arg_use_fake_data && 0) { // Temporarily DON'T shutdown so I can run multiple tests
        const char *shutdown_msg = "xx";
        sendto(sockfd, shutdown_msg, strlen(shutdown_msg), 0, (const struct sockaddr *)&server_addr, addr_len);
        close(sockfd);
    }

    // Write out remote hosts' counter values to disk for graphing (cpu idle%, load%, iowait%, num procs, etc.)
    if (rank == 0 && !arg_use_fake_data) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "./visualization/%d-%d-%d-%d-%d-%s", size, arg_benchmark, arg_exchanges, arg_depth, arg_nMax, HOST_LOGFILE_BASE_NAME);
        write_host_counter_data(buffer);
    }

    // Compute and print local (BFs') stats to log file
    // ALL BFs must reach this
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "./visualization/%d-%d-%d-%d-%d-%s-%s", size, arg_benchmark, arg_exchanges, arg_depth, arg_nMax, arg_local_logfile, LOCAL_LOGFILE_BASE_NAME);
    compute_benchmarking_data(arg_benchmark, arg_depth, arg_nMax, arg_debug, arg_use_fake_data, buffer);

    // Housekeeping
    MPI_Finalize();
    free_benchmarking_data();
    destroyNeighbourhoodView(ns, arg_depth, arg_nMax);

    return 0;
}
