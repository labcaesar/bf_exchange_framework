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
void initialize_benchmarking(int passed_size, int passed_rank, int passed_arg_exchanges, int passed_EXCHANGED_HOST_DATA_COUNT);

/**
 *  @find_available_filename
 *  
 *  Check if identically named file exists already and return filename with incremented number if it does
 *
 *  @base:     Base filename to look for / increment
 *
 */
char *find_available_filename(const char *base);

/**
 *  @benchmark_log_time
 *  
 *  Capture successive timestamps for benchmarking.
 *
 *  @iteration:     Iteration number (out of total arg_exchanges)
 *
 */
void benchmark_log_time(int iteration);

/**
 *  @log_remote_counters
 *  
 *  Log gathered/received values in an internal buffer (as a string, not merely series of values) for later printing
 *
 *  @recBuffer:     Pointer to leader's (someone's) receive buffer
 *
 */
void log_remote_counters(long *recBuffer, int EXCHANGED_HOST_DATA_COUNT);

/**
 *  @write_host_counter_data
 *  
 *  Write-out the exchanged data we previously stored as strings to a file.
 *
 *  @filename:     Base file name to try and use, can handle if already exists, must be null-terminated.
 *
 */
void write_host_counter_data(char *filename);

/**
 *  @write_local_counter_data
 *  
 *  Write-out the passed list of strings to file. This is used to record statistics collected from BFs.
 *
 *  @filename:      Base file name to try and use, can handle if already exists, must be null-terminated.
 *  @local_data:    List of null-terminated strings containing statistics we wish to print.
 *
 */
void write_local_counter_data(char *filename, char **local_data);

/**
 *  @fetch_benchmarking_counters
 *  
 *  Fetch local (BFs') performance stats and store in benchmarking struct.
 *
 *  @exchange:      Iteration number (out of total arg_exchanges)
 *
 */
void fetch_benchmarking_counters(int exchange);

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
void compute_benchmarking_data(int arg_benchmark, int arg_depth, int arg_nMax, int arg_debug, int arg_use_fake_data, char *filename);

/**
 *  @free_benchmarking_data
 *  
 *  Free all the buffers we used in the above functions
 *
 */
void free_benchmarking_data();