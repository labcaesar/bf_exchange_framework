// SERVER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

// IMPORTANT: Make SURE these align with the values defined in other source code
#define PORT 56789 
#define BUFFER_SIZE 1024
#define DATAPOINTS_TO_SEND 8
#define BASE_PATH "/sys/class/infiniband/"
#define MAX_PATHS 16
#define MAX_PATH_LEN 256

/**
 *  @read_long_from_file
 *  
 *  Read a single long from the specified file
 *
 *  @path:      File to read lone from.
 *
 */
long read_long_from_file(char *path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    long val = 0;
    fscanf(f, "%ld", &val);
    fclose(f);
    return val;
}

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to all interfaces
    server_addr.sin_port = htons(PORT);        // Port

    // Bind socket to address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", PORT);

    // Determine paths to network counters we will need to read and how many there are
    DIR *dir;
    struct dirent *entry;
    char rcv_paths[MAX_PATHS][MAX_PATH_LEN];
    char xmit_paths[MAX_PATHS][MAX_PATH_LEN];
    int count = 0;

    dir = opendir(BASE_PATH);
    if (!dir) {
        perror("opendir failed");
        return 1;
    }

    // Look at each dir, check if they contain counter
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "mlx5_", 5) != 0) {continue;}

        char rcv_path[MAX_PATH_LEN], xmit_path[MAX_PATH_LEN];

        snprintf(rcv_path, MAX_PATH_LEN, "%s%s/ports/1/counters/port_rcv_data", BASE_PATH, entry->d_name);
        snprintf(xmit_path, MAX_PATH_LEN, "%s%s/ports/1/counters/port_xmit_data", BASE_PATH, entry->d_name);

        // Just to be sure
        if (access(rcv_path, R_OK) == 0 && access(xmit_path, R_OK) == 0) {
            strncpy(rcv_paths[count], rcv_path, MAX_PATH_LEN);
            strncpy(xmit_paths[count], xmit_path, MAX_PATH_LEN);
            count++;

            if (count >= MAX_PATHS) {break;}
        }
    }

    closedir(dir);

    // Buffer to place results into and send
    long sendBuffer[DATAPOINTS_TO_SEND];
    long rcv, xmit, cpu_user, cpu_nice, cpu_system, cpu_idle, cpu_iowait, cpu_irq, cpu_softirq, cpu_steal, cpu_guest, cpu_guest_nice, ctxt, procs_running, procs_blocked;

    while(1){
        // sit (sleep) and wait to receive a message from my BlueField
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n > 1){ //is this a regular request for data or are they telling me to shut down?
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            printf("Received shutdown message from IP: %s\n", client_ip);
            break;
        }

        // Deal with multiple network traffic counters (variable number of counters)
        for(int i = 0; i < count; i++){
            rcv += read_long_from_file(rcv_paths[i]);
        }

        for(int i = 0; i < count; i++){
            xmit += read_long_from_file(xmit_paths[i]);
        }

        // Read perf data
        FILE *fp = fopen("/proc/stat", "r");
        if (!fp) {
            perror("fopen /proc/stat failed");
            return -1;
        }

        // Extract relevant values
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "cpu ", 4) == 0) {
                sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", &cpu_user, &cpu_nice, &cpu_system, &cpu_idle, &cpu_iowait, &cpu_irq, &cpu_softirq, &cpu_steal, &cpu_guest, &cpu_guest_nice);
            } else if (strncmp(line, "ctxt", 4) == 0) {
                sscanf(line, "ctxt %ld", &ctxt);
            } else if (strncmp(line, "procs_running", 13) == 0) {
                sscanf(line, "procs_running %ld", &procs_running);
            } else if (strncmp(line, "procs_blocked", 13) == 0) {
                sscanf(line, "procs_blocked %ld", &procs_blocked);
            }
        }

        fclose(fp);

        // Send:
        sendBuffer[0] = cpu_idle;
        sendBuffer[1] = cpu_user+cpu_nice+cpu_system+cpu_irq+cpu_softirq+cpu_steal+cpu_guest+cpu_guest_nice; // cpu time spent "working"
        sendBuffer[2] = cpu_iowait;
        sendBuffer[3] = rcv;
        sendBuffer[4] = xmit;
        sendBuffer[5] = ctxt;
        sendBuffer[6] = procs_running;
        sendBuffer[7] = procs_blocked;

        sendto(sockfd, sendBuffer, DATAPOINTS_TO_SEND*sizeof(long), 0, (struct sockaddr *)&client_addr, addr_len);

        rcv = 0;
        xmit = 0;
    }

    close(sockfd);
    return 0;
}
