#include "mpi-neighbour.h"
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
               int numDataPerRank, int iteration, int numElems, int debug) {
  int tempRank, nSize;

  MPI_Comm_size(n, &nSize);
  MPI_Comm_rank(n, &tempRank);

  // Prints wich ranks reach this code and which rank the user wants to print
  // buffer contents
  if (debug >= 3) {
    printf("Iteration: %d, tempRank: %d, desiredRank: %d\n", iteration,
           tempRank, desiredRank);
  }

  if (desiredRank == tempRank) {
    printf("\nIteration: %d numElems: %d nSize: %d\n", iteration, numElems,
           nSize);
    for (int i = 0; i < nSize; i++) {
      // Prints the data chunks "id"
      printf("Data chunk %d: ", i);
      for (int j = 0; j < numDataPerRank; j++) {
        // indices through the flattened array to print the data pretaining to a
        // specific "rank"
        printf("%lu ", dataBuffer[i * numDataPerRank + j]);
      }
      printf("\n");
    }
  }
}

/**
 *  @makeLeaderComm
 *  Takes all oldComms and creates one new comm containing all rank 0 (leader)
 *  processes
 *
 *  @oldComm:  The old comm group from which leaders are selected
 *  @*newComm: The new comm group containing only leaders
 *
 */
void makeLeaderComm(MPI_Comm oldComm, MPI_Comm *newComm) {
  // Ensures only valid comms are placed in the group, all comms must reach
  // MPI_Comm_split as it operates on MPI_COMM_WORLD so the invalid comms can't
  // just leave
  if (oldComm == MPI_COMM_NULL) {
    *newComm = MPI_COMM_NULL;
  }

  int oldRank, leader, worldRank;

  // Only grabs rank of valid comms, MPI_COMM_NULL comms crash if MPI_Comm_rank
  // is used on them
  if (oldComm != MPI_COMM_NULL) {
    MPI_Comm_rank(oldComm, &oldRank);
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  // Assigns all processes with rank 0 (leaders of previous comm) the same
  // groupID for use in the split
  leader = (oldRank == 0) ? 0 : MPI_UNDEFINED;

  // Splits MPI_COMM_WORLD to create a small comm of only leaders
  MPI_Comm_split(MPI_COMM_WORLD, leader, worldRank, newComm);
}

/**
 *  @makeNewComm
 *  Takes the old comm and splits it into smaller comms based on the maximum
 *  specified neighbourhood size
 *
 *  @oldComm:  The old comm group that will be split
 *  @arg_nMax: The max neighbourhood size
 *  @*newComm: The new comm group adhereing to the neighbourhood properties
 *
 */
void makeNewComm(MPI_Comm oldComm, int arg_nMax, MPI_Comm *newComm) {
  // Removes all invalid comms setting their new group to MPI_COMM_NULL
  if (oldComm == MPI_COMM_NULL) {
    *newComm = MPI_COMM_NULL;
    return;
  }

  int oldRank, oldSize, groupID, numGroups;
  if (oldComm != MPI_COMM_NULL) {
    MPI_Comm_rank(oldComm, &oldRank);
    MPI_Comm_size(oldComm, &oldSize);
  }

  // checks to ensure the size is worth spliting into smaller chunks
  if (oldSize > arg_nMax) {
    // Gets the total number of groups required to accomodate the max
    // neighbourhood size
    numGroups = ceil((float)oldSize / arg_nMax);
    // Gives each process a groupID based on it's previous rank
    groupID = oldRank % numGroups;
  } else {
    // Sets the id of all comms to 0 as the size of the comm is smaller than the
    // max neighbourhood size (a single gather can take place)
    groupID = 0;
  }

  int worldRank;
  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  // Splits the old comm group by group ID in order of their given worldRank
  MPI_Comm_split(oldComm, groupID, worldRank, newComm);
}

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
 *  @flag_allGather: Flag indicating whether or not all-gather will occur
 *
 */
void createNeighbourhoodView(MPI_Comm **n, NeighbourhoodShape *ns,
                             int arg_depth, int arg_nMax, int arg_benchmark,
                             int flag_allGather) {
  int nRank, nSize;
  ns->receiveCount = malloc((arg_depth + 1) * sizeof(int *));
  ns->offset = malloc((arg_depth + 1) * sizeof(int *));

  // Fill / initialize arrays
  for (int i = 0; i <= arg_depth; i++) { // Note the less-than-or-equal sign!
    // If we're at the last layer, and this layer is an allgather, treat
    // differently As in, the last layer's size isn't nMax, it's the size of the
    // allgather
    if (flag_allGather && i == arg_depth && ((*n)[i - 1] != MPI_COMM_NULL)) {
      MPI_Comm_size((*n)[i - 1], &nSize);
    } else {
      nSize = arg_nMax;
    }

    ns->receiveCount[i] = malloc(nSize * sizeof(int));
    if (ns->receiveCount[i] == NULL) {
      printf("Failed to allocate ns->receiveCount[i] with nSize=%d\n", nSize);
    }

    ns->offset[i] = malloc(nSize * sizeof(int));

    for (int j = 0; j < nSize; j++) {
      ns->receiveCount[i][j] = -1;
      ns->offset[i][j] = 0;
    }
  }

  // First recvcount for every rank at lowest depth is the number of bytes they
  // initially send
  ns->receiveCount[0][0] = arg_benchmark;

  // Gather neighbour counts
  for (int i = 0; i < arg_depth; i++) {
    if ((*n)[i] == MPI_COMM_NULL) {
      continue;
    }

    MPI_Comm_rank((*n)[i], &nRank);
    MPI_Comm_size((*n)[i], &nSize);

    // This will store the number of items the leader will receive from each
    // neighbour
    int *my_neighbour_counts = malloc(nSize * sizeof(int));
    memset(my_neighbour_counts, -1, nSize * sizeof(int));

    // Sum of all my previous neighbors (sum of row counts[i * (arg_nMax)])
    // This is how many items I will be sharing to my leader (or all neighbours
    // if it's an allgather)
    int sendingCount = 0;
    for (int j = 0; j < arg_nMax; j++) {
      if (ns->receiveCount[i][j] == -1) {
        break;
      }
      sendingCount += ns->receiveCount[i][j];
    }

    // Tell my neighbourhood leader (or everyone if an allgather) how many items
    // I am sharing
    if (flag_allGather && i == arg_depth - 1) {
      MPI_Allgather(&sendingCount, 1, MPI_INT, my_neighbour_counts, 1, MPI_INT,
                    (*n)[i]);
    } else {
      MPI_Gather(&sendingCount, 1, MPI_INT, my_neighbour_counts, 1, MPI_INT, 0,
                 (*n)[i]);
    }

    // Store how many items I (as a leader or allgather participant) will be
    // receiving from everyone
    if (!nRank || (flag_allGather && i == arg_depth - 1)) {
      for (int j = 0; j < nSize; j++) {
        ns->receiveCount[i + 1][j] = my_neighbour_counts[j];
        if (j != nSize - 1) {
          ns->offset[i + 1][j + 1] =
              my_neighbour_counts[j] + ns->offset[i + 1][j];
        }
      }
    }

    free(my_neighbour_counts);
  }

  return;
}

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
                              int arg_nMax) {
  for (int i = 0; i <= arg_depth; i++) { // Note the less-than-or-equal sign
    free(ns->receiveCount[i]);
    free(ns->offset[i]);
  }
  free(ns->receiveCount);
  free(ns->offset);
  return;
}

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
 *  @flag_allGather: Flag indicating whether or not all-gather will occur
 *
 */
void printNeighbourhoodView(MPI_Comm **n, NeighbourhoodShape *ns, int rank,
                            int arg_depth, int arg_nMax, int flag_allGather) {
  int nRank, nSize;

  // This will determine whether the last depth is an allgather layer or another
  // neighbourhood layer
  if (flag_allGather && ((*n)[arg_depth - 1] != MPI_COMM_NULL)) {
    MPI_Comm_size((*n)[arg_depth - 1], &nSize);
  } else {
    nSize = arg_nMax;
  }

  printf("worldRank %d: \n", rank);

  // Print recvcounts
  printf("counts: \n");
  for (int i = 0; i < arg_depth; i++) {
    printf("depth=%d: ", i);
    for (int j = 0; j < arg_nMax; j++) {
      printf("%d ", ns->receiveCount[i][j]);
    }
  }
  // Print last (highest) depth (could be allgather or neighbourhood)
  printf("depth=%d: ", arg_depth);
  for (int j = 0; j < nSize; j++) {
    printf("%d ", ns->receiveCount[arg_depth][j]);
  }

  // Print offsets
  printf("\noffsets: \n");
  for (int i = 0; i < arg_depth; i++) {
    printf("depth=%d: ", i);
    for (int j = 0; j < arg_nMax; j++) {
      printf("%d ", ns->offset[i][j]);
    }
  }
  // Print last (highest) depth (could be allgather or neighbourhood)
  printf("depth=%d: ", arg_depth);
  for (int j = 0; j < nSize; j++) {
    printf("%d ", ns->offset[arg_depth][j]);
  }

  // To make print-out more readable
  MPI_Barrier(MPI_COMM_WORLD);

  // Prints leader tier information
  for (int i = 0; i < arg_depth; i++) {
    // If not a participant in this layer, break out
    if ((*n)[i] == MPI_COMM_NULL) {
      break;
    }

    MPI_Comm_rank((*n)[i], &nRank);
    MPI_Comm_size((*n)[i], &nSize);

    // Let everyone in this layer know who else is involved
    int *ranks = malloc(nSize * sizeof(int));
    MPI_Allgather(&rank, 1, MPI_INT, ranks, 1, MPI_INT, (*n)[i]);

    // If I am leader in this layer, print my status and handle allgather layers
    if (flag_allGather &&
        i == arg_depth - 1) { // If I am at last layer, is it an allgather?
      printf("\n>>>worldRank %d - Tier %d leader (allGather) of worldRanks: ",
             rank, i);
      for (int j = 0; j < nSize; j++) {
        printf("%d ", ranks[j]);
      }
      printf("\n");

    } else if (!nRank) {
      printf("\n>>>worldRank %d - Tier %d leader of worldRanks: ", rank, i);
      for (int j = 0; j < nSize; j++) {
        printf("%d ", ranks[j]);
      }
      printf("\n");
    }
  }

  // To make print-out more readable
  MPI_Barrier(MPI_COMM_WORLD);
  return;
}

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
 *  @flag_allGather: Flag indicating whether or not all-gather will occur
 *  @debug:          The debug flag used for development
 *
 */
void createCommsArray(MPI_Comm **n, int *arg_depth, int *arg_nMax,
                      int *flag_allGather, int debug) {

  int worldRank, worldSize;
  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

  if (*arg_nMax == 1) {
    printf("Invalid use of parameter \"-n\"/\"--neighbourhood-size\", cannot "
           "equal 1\n");
    return;
  }

  // Creates a temporary neighbourhood size based on the desired depth for use
  // in fixing user values
  int tempN = (int)ceil((exp(log((double)worldSize) / (double)*arg_depth)));

  // Creates a temporary depth based on the desired neighbourhood size for use
  // in fixing user values
  int tempD = ((int)ceil(log((double)worldSize) / log((double)*arg_nMax)));

  // Determines the largest possible depth given the smallest possible
  // neighbourhood size
  int threshD = ((int)ceil(log((double)worldSize) / log((double)2)));

  if (worldRank == 0 && debug >= 1) {
    printf("!!!!!!tempD %d, arg_depth %d, threshD %d, arg_nMax %d, tempN %d\n",
           tempD, *arg_depth, threshD, *arg_nMax, tempN);
  }

  if (*arg_depth == 1) {
    *flag_allGather = 1;
    *n = malloc(sizeof(MPI_Comm));
    (*n)[0] = MPI_COMM_WORLD;
    *arg_nMax = worldSize;
    return;
  } else if (*arg_depth > 1 && *arg_nMax == -1) { // User only specifies d
    if (tempN != 1 && *arg_depth <= threshD) {
      *arg_nMax = tempN;
    } else if (tempN >= 2 || *arg_depth > threshD) {
      // Fixes arg_depth if the user specifies one that would cause for n to be
      // smaller than 2
      *arg_depth = ((int)ceil(log((double)worldSize) / log((double)tempN)));
      *arg_nMax = tempN;
    }
  } else if (*arg_nMax > 1 && *arg_depth == -1) { // User only specifies n
    // defualts to all gather if the neighbourhood size is set larger than the
    // worldSize
    if (*arg_nMax > worldSize) {
      *flag_allGather = 1;
      *arg_depth = 1;
    } else {
      *arg_depth = tempD;
    }
  } else if (*arg_nMax > 1 && *arg_depth > 1) { // User specifies both n and d
    // Corrects d and n values based on thresholds
    if (*arg_depth > tempD && *arg_nMax < worldSize) {
      *arg_depth = tempD;
      *flag_allGather = 1;
    } else if (*arg_nMax >= worldSize) {
      *flag_allGather = 1;
      *arg_depth = 1;
    } else {
      *flag_allGather = 1;
    }
  } else {
    printf("no specified values for \"-n\"/\"--neighbourhood-size\" or "
           "\"-d\"/\"--depth\"\n");
    return;
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  // Based on the depth creates an array of comms
  *n = malloc((*arg_depth) * sizeof(MPI_Comm));

  // Debug
  if (*n == NULL) {
    printf("Memory allocation failed!\n");
  }

  for (int i = 0; i < *arg_depth; i++) {
    (*n)[i] = MPI_COMM_NULL;
  }

  MPI_Comm tempLeaderComm = MPI_COMM_NULL;

  // First round of making a new comm uses MPI_COMM_WORLD to create first
  // neighbourhoods
  makeNewComm(MPI_COMM_WORLD, *arg_nMax, &(*n)[0]);

  for (int i = 0; i < *arg_depth - 1; i++) {

    makeLeaderComm((*n)[i], &tempLeaderComm);
    if (tempLeaderComm != MPI_COMM_NULL) {
      // If the all gather flag is set and the final depth is reached, a copy of
      // the leader comm is used instead of a new neighbourhood group to enable
      // all gather to happen between all remaining local leaders
      if (*flag_allGather == 1 && i == *arg_depth - 2) {
        MPI_Comm_dup(tempLeaderComm, &(*n)[i + 1]);
      } else {
        if ((i + 1) < *arg_depth) {
          makeNewComm(tempLeaderComm, *arg_nMax, &(*n)[i + 1]);
        } else {
          (*n)[i + 1] = MPI_COMM_NULL;
        }
      }
    }

    if ((*n)[i] == MPI_COMM_NULL) {
      (*n)[i + 1] = MPI_COMM_NULL;
    }
  }
}

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
 *  @flag_allGather: Flag indicating whether or not all-gather will occur
 *  @debug:          The debug flag used for development
 *
 */
void gatherCommData(MPI_Comm **n, NeighbourhoodShape *ns, long **recBuffer,
                    long *sendBuffer, int arg_nMax, int arg_depth,
                    int arg_benchmark, int flag_allGather, int debug) {

  int nRank, nSize, worldRank, worldSize, recCount, recSize, sendCount,
      sendBufferSize;

  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  // If the depth is 1, then a simple all gather will take place
  if ((arg_depth == 1 || arg_nMax >= worldSize) && flag_allGather == 1) {

    // Requests to track communications
    MPI_Request requests[1];

    // Allocates the max size for each process to recieve data
    *recBuffer = malloc(arg_benchmark * worldSize * sizeof(long));

    // TONY change this to allgather instead (and remove the waitall)
    MPI_Iallgather(sendBuffer, arg_benchmark, MPI_LONG, *recBuffer,
                   arg_benchmark, MPI_LONG, (*n)[0], &requests[0]);

    // Waits to exit the function until everyone recieves the required data
    MPI_Waitall(1, requests, MPI_STATUSES_IGNORE);

    return;
  } else {
    // Temporary buffer created to allow leaders to transfer data for sending
    // later
    long *currentSendBuffer;

    for (int i = 0; i < arg_depth; i++) {
      // reset the send count for each process
      sendCount = 0;

      // break out of the loop if the comm no longer exists
      if ((*n)[i] == MPI_COMM_NULL) {
        break;
      }

      MPI_Comm_size((*n)[i], &nSize);
      MPI_Comm_rank((*n)[i], &nRank);

      // every process needs to know how many items they will send for variable
      // gather
      for (int j = 0; j < arg_nMax && ns->receiveCount[i][j] != -1; j++) {
        sendCount += ns->receiveCount[i][j];
      }

      sendBufferSize = sendCount * sizeof(long);
      currentSendBuffer = malloc(sendBufferSize);

      if (i == 0) {
        memcpy(currentSendBuffer, sendBuffer, sendBufferSize);
      } else {
        memcpy(currentSendBuffer, *recBuffer, sendBufferSize);
      }

      // Rank one also needs to know how much it's gonna recieve
      if (nRank == 0) {
        // free the recBuffer on subsequant iterations to ensure malloc is ok
        if (i != 0) {
          free(*recBuffer);
        }

        recCount = 0;
        for (int j = 0; j < arg_nMax && ns->receiveCount[i + 1][j] != -1; j++) {
          recCount += ns->receiveCount[i + 1][j];
        }
        recSize = recCount * sizeof(long);
        *recBuffer = malloc((size_t)recSize);
      }

      // Base case, always gather until right before last layer in comm
      if (i != arg_depth - 1) {
        MPI_Gatherv(currentSendBuffer, sendCount, MPI_LONG, *recBuffer,
                    ns->receiveCount[i + 1], ns->offset[i + 1], MPI_LONG, 0,
                    (*n)[i]);
      }

      // If there is no all gather, do one last regular gather to one leader
      if (i == arg_depth - 1 && flag_allGather != 1) {
        MPI_Gatherv(currentSendBuffer, sendCount, MPI_LONG, *recBuffer,
                    ns->receiveCount[i + 1], ns->offset[i + 1], MPI_LONG, 0,
                    (*n)[i]);

        // if the final depth is reached and the all gather flag is set, then a
        // variable all gather operation takes place between the remaining local
        // leaders
      } else if (i == arg_depth - 1 && flag_allGather == 1) {
        // Requests to track communications
        MPI_Request requests[1];

        if (worldRank == 0) {
          free(*recBuffer);
        }

        *recBuffer = malloc(arg_benchmark * worldSize * sizeof(long));

        MPI_Iallgatherv(currentSendBuffer, sendCount, MPI_LONG, *recBuffer,
                        ns->receiveCount[i + 1], ns->offset[i + 1], MPI_LONG,
                        (*n)[i], &requests[0]);
        MPI_Waitall(1, requests, MPI_STATUSES_IGNORE);
      }

      if (debug >= 1 && (i != arg_depth - 1)) {
        printData((*n)[i], 0, *recBuffer, arg_benchmark, i, sendCount, debug);
      }

      // Destroys the temporary buffer once a process is done with it
      free(currentSendBuffer);
    }
  }

  return;
}
