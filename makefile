PLACEHOLDER.

TODO - Makefile:
-We should have a recipe to compile for both BF and x86, and some example run commands for allgather, hybrid, and neighbourhoods (probably synthetic), and at least one that uses real BFs

TODO - Code:
-Rename them to have reasonable names
-Remove constant printing of neighbourhood view struct in benchmark-longs
-Fix logfile error on x86 when using -b
-Add -l / --logfile to printhelp function
-Implement the remote data logfile option I allude to with the DEFINE for it
-Find a way to run this without benchmarking (to isolate mem/cpu usage spent on actual exchange and spent on benchmarking)