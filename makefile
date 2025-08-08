all:
	mpicc -g -Wall -O0 -o mpi-bench benchmark/mpi-benchmark-longs.c neighbour/mpi-neighbour.c benchmark/benchmarking.c -lm 

allgather:
	mpicc -o allgather/mpi-allgather allgather/mpi-allgather.c

bench:
	mpicc -o mpi-benchmark-longs benchmark/mpi-benchmark-longs.c neighbour/mpi-neighbour.c -lm
