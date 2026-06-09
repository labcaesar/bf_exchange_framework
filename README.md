# SmartNIC Data Exchange Framework

_A flexible data exchange system designed to run on BlueField DPUs which can be used to extract information from hosts exchange it with others_<br><br>

### What is this project?

This project is a system for collecting arbitrary data from traditional hosts (via an RPC-like UDP server process) and exchanging that data with other SmartNICs.
Three network topologies are supported:

- Flat / Allgather (all participants receive a complete copy of data)
- Hybrid (participants at a specified level in the hierarchy receive a complete copy of data)
- Neighbourhoods (only the participant at the top of the hierarchy has a complete copy of data)

These are configured by specifying a maximum neighbourhood size and, optionally, a depth at which all leaders should have complete copies of data.

This framework was designed as a generic way of gathering and distributing data with the goal of supporting future applications which use host-gathered statistics to make rack-, row-, or cluster-level decision with minimal host interference.

Examples of these include RAS applications, shared resource access control, work stealing / load balancing, power management, and cluster monitoring among others.<br><br>

### How can I try the framework?

Dependencies:

- OpenMPI

The following recipes—found in the included makefile—may be used to compile and run the framework alone:

<pre>
recipe one
recipe two
etc.
</pre>

Note that the executables must be compiled for the systems they are destined to run on (BlueFields use ARM cores, not x86).
<br><br>

##### Command-line options:

The following explanation of command-line options can also be displayed with `-h / --help`:
| Option | Type | Description | Default | Valid Range | Required? |
|------|------|------|------|------|------|
| -e / --exchanges | \<int> | Number of exchange rounds to conduct between BlueFields. | 4 | >1 | No |
| -n / --neighbourhood-size | \<int> | Maximum neighborhood size. If n > number of processes, allgather will occur at a depth of 1 regardless of what d is, if n == number of processes, a single neighbourhood will be used with no all gather. | 2 | >1 | No<b>\*</b> |
| -d / --depth | \<int> | The depth at which allgather will occur. Setting this alone controls the neighbourhood size, defaulting to a value that accommodates n = 2 if the depth is too big. NOTE: If both n and d are set, neighbourhood size of n will be used if possible, if neighbourhoods of n cause the depth value to change, depth will be reduced to enable all gather on the final layer. | N/A | >1<b>\*\*</b> | No<b>\*</b> |
| -v / --verbose | \<int> | Print debugging messages, varying in granularity. | 0 | 0-10 | No |
| -h / --help | Flag | Show help message. | N/A | N/A | No |
| -b / --benchmark | \<int> | Number of "items" (long-type values) each BlueField should send per exchange. This option should be used for benchmarking data exchange speeds (meaningless information is exchanged). If it is omitted, the benchmark will exchange _actual_ information retrieved from hosts. | N/A | >1 | No |
| -l / --logfile | \<String> | An optional string to include in the local (BlueField statistics) log files. If omitted, the parameters which define the architecture and other command-line options will make up the logfile. Note that if a duplicate logfile would be created, an incrementing number will be appended to the end to avoid collisions. | N/A | N/A | No |

_<b>\*</b>At least one of these must be specified._
_<b>\*\*</b>If an invalid depth is supplied (it is not possible to create neighbourhoods of maximum size to that depth, e.g. -n 2, --np 16, -d 8), the depth will be corrected such that an allgather occurs between the members of the last neighbourhood._
<br><br>
Please note that only the maximum neighbourhood size should be specified for a pure neighbourhood topology, both for a hybrid, and only depth (of 1) for a flat allgather, though other combinations are possible. <br><br>

##### Overview of files included in distribution:

- mpi-benchmark-longs.c: The main source file in which mpi-neighbour.h's and benchmarking.h's functions are invoked and the framework is initialized. This file also parses arguments and contains helper functions used in other source files.
- mpi-neighbour.c: Implementation of core framework functions (neighbourhood creation, data exchange function, network view creation, etc.)
- mpi-neighbour.h: Header file for core framework functions
- benchmarking.c: Implementation of auxiliary benchmarking and logging functions (statistic calculation, file writing, counter retrieval, etc.)
- benchmarking.h: Header file for auxiliary benchmarking and logging functions
- server_x86.c: Implementation of host-side, RPC-like UDP server interfaced-with by BlueFields
- makefile: Makefile to compile and run framework
- README\.md: This document
- LICENSE\.md: License governing use of this project
  <br><br>

### How can this be integrated into my own project?

You are free to use this project's source code as permitted by the license found in this repository.
We foresee that integrations of this project will require edits to:

- The host-side server process to specify the data to gather
- The BlueField-side `update_global_view()` function to act appropriately given received data
- The defines dictating the quantity of data both exchanged between BlueFields and received from hosts
  <br><br>

### Notes

Development of this framework is on-going; features and bugfixes will be delivered once thoroughly tested, though some bugs may persist.
We would ask that you contact us if you encounter any bugs so that we can confirm and correct them.
<br><br>

### Contact

Anthony Sicoie - anthony.sicoie@queensu.ca

Zackary Savoie - 19zs51@queensu.com
