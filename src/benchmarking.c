#include <math.h>
#include <mpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

static int rank;
static int arg_exchanges;
static int size;
static char** remote_data;
static int remote_data_count = 0;
static int EXCHANGED_HOST_DATA_COUNT;

// These will be provided elsewhere
extern void print_buffer_long(long *data, int len);
extern void print_buffer_long_long(long long *data, int len);

// Struct to keep track of timestamps and counter values for benchmarking purposes
struct stats_struct {
        long *time_1;
        long *time_2;
        long *time_3;
        long long *rcv;
        long long *xmit;
};

struct timespec ts;
struct stats_struct stats;


/**
 *  @initialize_benchmarking
 *  
 *  Allocate memory for BFs to record statistics and save copies of 
 *  important parameters used by later functions in this header.
 *
 *  @passed_size:                       Size of MPI_COMM_WORLD (number of BFs)
 *  @passed_rank:                       Rank of this BF / process in MPI_COMM_WORLD
 *  @passed_arg_exchanges:              Number of exchange (how many times will 
 *                                      leaders gather updated statistics)
 *  @passed_EXCHANGED_HOST_DATA_COUNT:  Number of values shared between BlueFields 
 *                                      when gathering
 *
 */
void initialize_benchmarking(int passed_size, int passed_rank, int passed_arg_exchanges, int passed_EXCHANGED_HOST_DATA_COUNT){
    // These are reused, lets save a copy of them
    rank = passed_rank;
    arg_exchanges = passed_arg_exchanges;
    size = passed_size;
    EXCHANGED_HOST_DATA_COUNT = passed_EXCHANGED_HOST_DATA_COUNT;

    remote_data = malloc(size*arg_exchanges*sizeof(char *));

    stats.time_1 = malloc(sizeof(long) * size * arg_exchanges);
    stats.time_2 = malloc(sizeof(long) * size * arg_exchanges);
    stats.time_3 = malloc(sizeof(long) * size * arg_exchanges);
    memset(stats.time_1, 0, sizeof(long) * size * arg_exchanges);
    memset(stats.time_2, 0, sizeof(long) * size * arg_exchanges);
    memset(stats.time_3, 0, sizeof(long) * size * arg_exchanges);

    stats.rcv = malloc(sizeof(long long) * size * arg_exchanges);
    stats.xmit = malloc(sizeof(long long) * size * arg_exchanges);
    memset(stats.rcv, 0, sizeof(long long) * size * arg_exchanges);
    memset(stats.xmit, 0, sizeof(long long) * size * arg_exchanges);

    return;
}

/**
 *  @find_available_filename
 *  
 *  Check if identically named file exists already and return filename with incremented number if it does
 *
 *  @base:     Base filename to look for / increment
 *
 */
char *find_available_filename(const char *base) {
    char *dot = strrchr(base, '.');
    char name[256];

    if (dot == NULL) {
        // No extension found, treat whole base as filename
        snprintf(name, sizeof(name), "%s", base);
        dot = name + strlen(name); // point to null terminator
    } else {
        strncpy(name, base, dot - base);
        name[dot - base] = '\0'; // terminate string at dot
    }

    const char *ext = dot; // includes the dot

    char candidate[300];
    int index = 0;

    // Check base file itself first
    if (access(base, F_OK) != 0) {
        return strdup(base);
    }

    // Increment filename until one does not exist
    do {
        snprintf(candidate, sizeof(candidate), "%s%d%s", name, index, ext);
        index++;
    } while (access(candidate, F_OK) == 0);

    return strdup(candidate); // allocate result for caller to use
}

/**
 *  @benchmark_log_time
 *  
 *  Capture successive timestamps for benchmarking.
 *
 *  @iteration:     Iteration number (out of total arg_exchanges)
 *
 */
void benchmark_log_time(int iteration){
    clock_gettime(CLOCK_MONOTONIC, &ts);

    if (!stats.time_1[rank * arg_exchanges + iteration]) {
        stats.time_1[rank * arg_exchanges + iteration] = ts.tv_sec * 1000000000L + ts.tv_nsec;
    } else if (!stats.time_2[rank * arg_exchanges + iteration]) {
        stats.time_2[rank * arg_exchanges + iteration] = ts.tv_sec * 1000000000L + ts.tv_nsec;
    } else if (!stats.time_3[rank * arg_exchanges + iteration]) {
        stats.time_3[rank * arg_exchanges + iteration] = ts.tv_sec * 1000000000L + ts.tv_nsec;
    } else {
        printf("TIME LOG ERROR!\n");
    }

    return;
}

/**
 *  @log_remote_counters
 *  
 *  Log gathered/received values in an internal buffer (as a string, not merely series of values) for later printing
 *
 *  @recBuffer:     Pointer to leader's (someone's) receive buffer
 *
 */
void log_remote_counters(long *recBuffer) {
    // Assumes that we are a global leader (i.e. we have data from all remote hosts)
    for (int rank_n = 0; rank_n < size; rank_n++) { 
        char temp[32];
        int line_length = snprintf(temp, sizeof(temp), "%d,", remote_data_count);

        // Determine length of line
        for (int index = 0; index < EXCHANGED_HOST_DATA_COUNT; index++) {
            long number = recBuffer[rank_n * EXCHANGED_HOST_DATA_COUNT + index];
            char temp[32]; // probably logn enough?
            int len = snprintf(temp, sizeof(temp), "%ld,", number);
            line_length += len;
        }

        // Actually write the line
        char *line = malloc(line_length + 1); // for '\0'
        char *ptr = line;
        for (int index = 0; index < EXCHANGED_HOST_DATA_COUNT; index++) {
            long number = recBuffer[rank_n * EXCHANGED_HOST_DATA_COUNT + index];
            ptr += sprintf(ptr, "%ld,", number);  // safe since we already computed size
        }

        *--ptr = '\0';

        remote_data[remote_data_count++] = line;
    }
    return;
}

/**
 *  @write_host_counter_data
 *  
 *  Write-out the exchanged data we previously stored as strings to a file.
 *
 *  @filename:     Base file name to try and use, can handle if already exists, must be null-terminated.
 *
 */
void write_host_counter_data(char *filename) {
    
    FILE *fp = fopen(find_available_filename(filename), "w");
    if (!fp) {
        printf("\n\nFailed to open host counter file for write\n\n");
        return;
    }

    for (int i = 0; i < remote_data_count; i++) {
        fprintf(fp, "%s\n", remote_data[i]);
    }

    fclose(fp);

    return;
}

/**
 *  @write_local_counter_data
 *  
 *  Write-out the passed list of strings to file. This is used to record statistics collected from BFs.
 *
 *  @filename:      Base file name to try and use, can handle if already exists, must be null-terminated.
 *  @local_data:    List of null-terminated strings containing statistics we wish to print.
 *
 */
void write_local_counter_data(char *filename, char **local_data) {

    // Write to file
    FILE *fp = fopen(find_available_filename(filename), "w");
    if (!fp) {
        printf("\n\nFailed to open host counter file for write\n\n");
        return;
    }

    for (int i = 0; i < size; i++) {
        fprintf(fp, "%s\n", local_data[i]);
    }

    fclose(fp);

    return;
}

/**
 *  @fetch_benchmarking_counters
 *  
 *  Fetch local (BFs') performance stats and store in benchmarking struct.
 *
 *  @exchange:      Iteration number (out of total arg_exchanges)
 *
 */
void fetch_benchmarking_counters(int exchange){
    // Identify all ports with valid counters we can read from

    // Could we do this once? maybe. 
    // Could hardware change mid-run? maybe. 
    // Is the speed of this function important? absolutely not.

    DIR *dir;
    struct dirent *entry;
    char rcv_paths[32][1024]; // Should be enough?
    char xmit_paths[32][1024];
    int count = 0;

    char *base_path = "/sys/class/infiniband/";

    dir = opendir(base_path);
    if (!dir) { // Maybe running locally or otherwise on a system without IB
        // Regardless, record as 0 and move on.
        stats.rcv[rank * arg_exchanges + exchange] = 0;
        stats.xmit[rank * arg_exchanges + exchange] = 0;
        return;
    }

    // Look at each dir, check if they contain counter
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "mlx5_", 5) != 0) {continue;} // Look at all mlx5_* dirs in base folder

        char rcv_path[1024], xmit_path[1024]; // should be enough?

        // check if they have counters we care about
        snprintf(rcv_path, 1024, "%s%s/ports/1/counters/port_rcv_data", base_path, entry->d_name);
        snprintf(xmit_path, 1024, "%s%s/ports/1/counters/port_xmit_data", base_path, entry->d_name);

        // To be sure we can read them, then store full path
        if (access(rcv_path, R_OK) == 0 && access(xmit_path, R_OK) == 0) {
            strncpy(rcv_paths[count], rcv_path, 1024);
            strncpy(xmit_paths[count], xmit_path, 1024);
            count++;

            if (count >= 32) {break;}
        }
    }

    closedir(dir);

    // Read total bytes sent and received across all ports
    long long val = 0;
    for(int i = 0; i < count; i++){
        FILE* f = fopen(rcv_paths[i], "r");
        if (!f) {perror("can't open port!"); exit(-1);}
        fscanf(f, "%lld", &val);
        stats.rcv[rank * arg_exchanges + exchange] += val;
        fclose(f);

        f = fopen(xmit_paths[i], "r");
        if (!f) {perror("can't open port!"); exit(-1);}
        fscanf(f, "%lld", &val);
        stats.xmit[rank * arg_exchanges + exchange] += val;
        fclose(f);
    }

    // RAM / Compute:
    //  ps -eo pid,ppid,comm,%cpu,%mem | grep "name_of_mpi_program_here" to see
    //  cpu and mem "percents" (kinda vague) Could look at "/proc/meminfo" and
    //  "/proc/stat" for more detailed stats

    return;
}

/**
 *  @compute_benchmarking_data
 *  
 *  Compute statistics like bytes received/transmitted, time spent in different parts of code, etc. and print to log file. 
 *  This functions requires that local benchmarking data has actually been recorded using fetch_benchmarking_counters() and
 *  benchmark_log_time().
 *
 *  @arg_benchmark:         Number of data (longs) sent by each participating BF
 *  @arg_depth:             Depth of neighbourhood hierarchy 
 *  @arg_nMax:              Maximum neighbourhood size
 *  @arg_debug:             Specifies verbosity of debug printouts
 *  @arg_use_fake_data:     Flag indicating that synthetic data was used instead of data gathered by BFs from remote hosts
 *  @filename:              Filename to try and write out benchmarking logs to, can handle if already exists, must be null-terminated
 *
 */
void compute_benchmarking_data(int arg_benchmark, int arg_depth, int arg_nMax, int arg_debug, int arg_use_fake_data, char *filename){
    // Gather benchmarking values stored in each rank's struct
    MPI_Gather(&(stats.rcv[rank * arg_exchanges]), arg_exchanges,
                            MPI_LONG_LONG, stats.rcv, arg_exchanges, MPI_LONG_LONG, 0,
                            MPI_COMM_WORLD);
    MPI_Gather(&(stats.xmit[rank * arg_exchanges]), arg_exchanges,
                            MPI_LONG_LONG, stats.xmit, arg_exchanges, MPI_LONG_LONG, 0,
                            MPI_COMM_WORLD);
    MPI_Gather(&(stats.time_1[rank * arg_exchanges]), arg_exchanges,
                            MPI_LONG, stats.time_1, arg_exchanges, MPI_LONG, 0,
                            MPI_COMM_WORLD);
    MPI_Gather(&(stats.time_2[rank * arg_exchanges]), arg_exchanges,
                            MPI_LONG, stats.time_2, arg_exchanges, MPI_LONG, 0,
                            MPI_COMM_WORLD);
    MPI_Gather(&(stats.time_3[rank * arg_exchanges]), arg_exchanges,
                            MPI_LONG, stats.time_3, arg_exchanges, MPI_LONG, 0,
                            MPI_COMM_WORLD);

    if (rank == 0) {
        long avg_host_time[size + 1], avg_exchange_time[size + 1]; // Space for per-rank averages AND overall averages (hence +1)
        long long total_data_rcv = 0;
        long long total_data_xmit = 0;

        memset(avg_host_time, 0, sizeof(avg_host_time));
        memset(avg_exchange_time, 0, sizeof(avg_exchange_time));

        for (int i = 0; i < size; i++) { // Compute the following for each rank
                // printf("rank %d exchange times: ", i);
                for (int j = 0; j < arg_exchanges; j++) { // Compute average data gathering and neighbourhood exchanging times
                    avg_host_time[i] += (stats.time_2[i * arg_exchanges + j] - stats.time_1[i * arg_exchanges + j]);
                    avg_exchange_time[i] += (stats.time_3[i * arg_exchanges + j] - stats.time_2[i * arg_exchanges + j]);
                    // printf("%ld ", (stats.time_3[i * arg_exchanges + j] - stats.time_2[i * arg_exchanges + j]));
                }
                // printf("\n\n");

                avg_host_time[i] /= (long)(arg_exchanges);
                avg_exchange_time[i] /= (long)(arg_exchanges);

                if (arg_debug >= 2) {
                    printf("==================================Rank %d==================================\n\nTimestamps:\n\ntime_1:\n", i);
                    print_buffer_long(&(stats.time_1[i * arg_exchanges]),arg_exchanges);
                    printf("time_2: \n");
                    print_buffer_long(&(stats.time_2[i * arg_exchanges]), arg_exchanges);
                    printf("time_3: \n");
                    print_buffer_long(&(stats.time_3[i * arg_exchanges]), arg_exchanges);

                    if(!arg_use_fake_data){ // Don't print host retrieval times or bytes sent/received by BFs if we're using synthetic data
                        printf("\nAvg. host retrieval time: %ldms\tAvg. data exchange time: ", avg_host_time[i] / 1000000L);
                        avg_exchange_time[i] < 1000000L ? printf("%ldns\n", avg_exchange_time[i]) : printf("%ldms\n", avg_exchange_time[i] / 1000000L);

                        printf("\n\nNetwork:\n\nrcv:\n");
                        print_buffer_long_long(&(stats.rcv[i * arg_exchanges]), arg_exchanges);
                        printf("xmit:\n");
                        print_buffer_long_long(&(stats.xmit[i * arg_exchanges]), arg_exchanges);
                        // Total bytes received and transmitted
                        printf("\nTotal rcv_bytes: %lld\tTotal xmit_bytes:%lld\n", stats.rcv[(i + 1) * arg_exchanges - 1] - stats.rcv[i * arg_exchanges], stats.xmit[(i + 1) * arg_exchanges - 1] - stats.xmit[i * arg_exchanges]);
                        // Average bytes received and transmitted per-exchange
                        printf("Per exchange avg. rcv_bytes: %lld\tPer exchange avg. xmit_bytes:%lld\n", (stats.rcv[(i + 1) * arg_exchanges - 1] - stats.rcv[i * arg_exchanges]) / arg_exchanges, (stats.xmit[(i + 1) * arg_exchanges - 1] - stats.xmit[i * arg_exchanges]) / arg_exchanges);
                    } else {
                        printf("\nAvg. host retrieval time: N/A\tAvg. data exchange time: ");
                        avg_exchange_time[i] < 1000000L ? printf("%ldns\n", avg_exchange_time[i]) : printf("%ldms\n", avg_exchange_time[i] / 1000000L);
                    }

                    printf("\n");
                }

                // Add this rank's averages to all-ranks average values
                avg_host_time[size] += avg_host_time[i];
                avg_exchange_time[size] += avg_exchange_time[i];
                // Total data sent and received by this rank
                total_data_rcv += stats.rcv[(i + 1) * arg_exchanges - 1] - stats.rcv[i * arg_exchanges];
                total_data_xmit += stats.xmit[(i + 1) * arg_exchanges - 1] - stats.xmit[i * arg_exchanges];
        }

        // Average across all ranks
        avg_host_time[size] /= size;
        avg_exchange_time[size] /= size;

        // Store local (BFs') stats as strings for later printing or logging
        char **local_data = malloc(size*sizeof(char*));

        for (int i = 0; i < size; i++) { // Go from collection of stats to list of strings to print
            // arg_use_fake_data, arg_benchmark, arg_exchanges, arg_depth, arg_nMax, size, rank, host_fetching_time, exchange_time, total rcv_bytes, total xmit_bytes
            local_data[i] = malloc(1024); // probably big enough?
            sprintf(local_data[i], "%d,%d,%d,%d,%d,%d,%d,%ld,%ld,%lld,%lld", arg_use_fake_data, arg_benchmark, arg_exchanges, arg_depth, arg_nMax, size, i, avg_host_time[i], avg_exchange_time[i], stats.rcv[(i + 1) * arg_exchanges - 1] - stats.rcv[i * arg_exchanges],stats.xmit[(i + 1) * arg_exchanges - 1] - stats.xmit[i * arg_exchanges]);
        }

        if (arg_debug >= 2) {
            printf("==================================Overall==================================\n\n");
            if(!arg_use_fake_data){ // Don't print host retrieval times or bytes sent/received by BFs if we're using synthetic data
                printf("Avg. host retrieval: %ldms\tAvg. data exchange time: ", avg_host_time[size] / 1000000L);
                avg_exchange_time[size] < 1000000L ? printf("%ldns\n", avg_exchange_time[size]) : printf("%ldms\n", avg_exchange_time[size] / 1000000L);
                printf("Total rcv_bytes: %lld\tTotal xmit_bytes: %lld\n", total_data_rcv, total_data_xmit);
            } else {
                printf("Avg. host retrieval: N/A\tAvg. data exchange time: ");
                avg_exchange_time[size] < 1000000L ? printf("%ldns\n", avg_exchange_time[size]) : printf("%ldms\n", avg_exchange_time[size] / 1000000L);
            }
            printf("\n");
            for (int i = 0; i < size; i++) {
                printf("%s\n",local_data[i]);
            }
        }

        // Write out to file
        write_local_counter_data(filename, local_data);
    }

    return;
}

/**
 *  @free_benchmarking_data
 *  
 *  Free all the buffers we used in the above functions
 *
 */
void free_benchmarking_data(){
    free(stats.time_1);
    free(stats.time_2);
    free(stats.time_3);
    free(stats.rcv);
    free(stats.xmit);
    free(remote_data);

    return;
}
