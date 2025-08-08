#include <mpi.h>

/* This struct holds the information needed by each rank to share their data
with leaders and gather data from non-leaders Specifically, receiveCount
holds the number of items expected to be received from each neighbour in
depths layers where a rank is a leader and offset holds the offsets required
to place this data in a buffer using variable gather collectives Note that
the first "depth" in receiveCount is just the number of items a rank will be
sending to it's first leader, i.e. the base amount of data each rank starts
with */
typedef struct {
  int **receiveCount;
  int **offset;
} NeighbourhoodShape;

/**
 *  @printData
 *  Prints the contents of a dataBuffer with indices for each chunk of data
 *
 *  @n:              The comm for which data will be printed
 *  @desiredRank:    The rank from the comm (n) that will print the data
 *  @*dataBuffer:    The buffer containing data that will be printed
 *  @numDataPerRank: How much data each rank has (dataSize)
 *  @iteration:      A debugging variable to inform the user on what iteration
 *                   of a loop the print happens
 *  @numElems:       The number of elements
 *  @debug:          The debug flag used for development
 *
 */
void printData(MPI_Comm n, int desiredRank, long *dataBuffer,
               int numDataPerRank, int iteration, int numElems, int debug);

/**
 *  @createNeighbourhoodView
 *  Creates 2D arrays containing the number of items each proces will send,
 *  along with their corresponding offsets for every given depth in the comms
 *  array
 *
 *  @**n:            Pointer to the comms array
 *  @*ns:            Pointer to the data structure containing the 2D arrays
 *  @arg_depth:      The depth of the comms array
 *  @arg_nMax:       The max neighbourhood size
 *  @arg_benchmark:  The ammount of data (number of longs) being passed
 *  @flag_allGather: Flag indicating whether or not MPI_Iallgatherv will occur
 *
 */
void createNeighbourhoodView(MPI_Comm **n, NeighbourhoodShape *ns,
                             int arg_depth, int arg_nMax, int arg_benchmark,
                             int flag_allGather);

/**
 *  @destroyNeighbourhoodView
 *  Destroys all of the arrays in the ns struct created by
 *  createNeighbourhoodView()
 *
 *  @*ns:            Pointer to the data structure containing the 2D arrays
 *  @arg_depth:      The depth of the comms array
 *  @arg_nMax:       The max neighbourhood size
 *
 */
void destroyNeighbourhoodView(NeighbourhoodShape *ns, int arg_depth,
                              int arg_nMax);

/**
 *  @printNeighbourhoodView
 *  This function will print the contents of a NeighbourhoodShape object,
 *  including: leader tiers of each rank, recv counts for each rank at each
 *  depth, and offsets for each rank at each depth
 *
 *  @**n:            Pointer to the comms array
 *  @*ns:            Pointer to the data structure containing the 2D arrays
 *  @rank:           The world rank printing it's 2D array data
 *  @arg_depth:      The depth of the comms array
 *  @arg_nMax:       The max neighbourhood size
 *  @flag_allGather: Flag indicating whether or not MPI_Iallgatherv will occur
 *
 */
void printNeighbourhoodView(MPI_Comm **n, NeighbourhoodShape *ns, int rank,
                            int arg_depth, int arg_nMax, int flag_allGather);

/**
 *  @createCommsArray
 *  This function creates an array of MPI_Comm types based on the specified
 *  depth and neighbourhood parameters specified by the user. If these values
 *  are incompatible, it sets them to values that allow for the user's specified
 *  neighbuorhood size to function.
 *
 *  @**n:            Pointer to the comms array
 *  @arg_depth:      The depth of the comms array
 *  @arg_nMax:       The max neighbourhood size
 *  @flag_allGather: Flag indicating whether or not MPI_Iallgatherv will occur
 *  @debug:          The debug flag used for development
 *
 */
void createCommsArray(MPI_Comm **n, int *arg_depth, int *arg_nMax,
                      int *flag_allGather, int debug);

/**
 *  @gatherCommData
 *  This function gathers the data across the entire system (all processes
 *  present in n) based on the communication heirarchy designed by the
 *  createCommsArray function
 *
 *  @**n:            Pointer to the comms array
 *  @*ns:            Pointer to the data structure containing 2D arrays
 *                   corresponding to recieve count and offset for use in
 *                   MPI_Gatherv
 *  @**recBuffer:    The buffer where each process will store data it recieves
 *  @*sendbuffer:    The buffer holding the data each process plans to send
 *  @arg_depth:      The depth of the comms array
 *  @arg_nMax:       The max neighbourhood size
 *  @arg_benchmark:  The ammount of data (number of longs) being passed
 *  @flag_allGather: Flag indicating whether or not MPI_Iallgatherv will occur
 *  @debug:          The debug flag used for development
 *
 */
void gatherCommData(MPI_Comm **n, NeighbourhoodShape *ns, long **recBuffer,
                    long *sendBuffer, int arg_nMax, int arg_depth,
                    int arg_benchmark, int flag_allGather, int debug);
