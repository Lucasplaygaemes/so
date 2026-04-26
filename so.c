/*     THIS PROGRM IS MADE ONLY FOR EDUCATIONAL PURPOUSES     */
/*     The objctive of this code is to make a program where   */
/*     Will change the priority of the program QEMU and it's  */
/*     Process to take the best possible of the system on use */
/*     It also will restore, it's made with a specific        */
/*     processor architecture in mind, may work on others.    */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <poll.h>

int global_qemu_pid = 0;

// Funtion change the terminal mode
void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(0, &new_termios);
    new_termios.c_lflag &= ~(ICANON | ECHO); // Deactivate the buffer and ECHO
    tcsetattr(0, TCSANOW, &new_termios);
}

// Funtion to manipulate system files cleanly
void write_sys_file(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", value);
        fclose(f);
    } else {
        perror("Error while trying to acess system resource.");
    }
}

// Function to get the QEMU PID automatically
int get_qemu_pid() {
    int pid;
    // "pidof" is a shell pipe command that will return the PID of a desired program called by it's name
    FILE *fp = popen("pidof -s qemu-system-x86_64", "r");
    if (fp == NULL) {
        printf("Error finding the process PID.\n");
        return -1;
    }
    if (fscanf(fp, "%d", &pid) != 1) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    return pid;
}

// Function to restore to the default
void restore_system(int sig) {
    printf("\n Restoring all system changes to default...\n");
    
    // Free the ram, Hugepages -> 0
    write_sys_file("/proc/sys/vm/nr_hugepages", "0");
    printf("RAM restored.\n");

    
    // Restore the IRQ Affinity, Mask 'fff' allows the 12 threads
    // Allows the kernel to use any core for interruptions again.
    write_sys_file("/proc/irq/default_smp_affinity", "fff");
    
     // Retuns QEMU to the default priority
    if (global_qemu_pid > 0) {
        struct sched_param param;
        sched_setscheduler(global_qemu_pid, SCHED_OTHER, &param);
        printf("QEMU (PID %d) returned to the default escalator.\n", global_qemu_pid);
    }
    printf("System clean, Exiting...\n");
    exit(0);
}

int main() {
    printf("--- PREPARING THE AMBIENT FOR THE MAX PERFORMACE ---\n");

    // Try to find the VM 
    global_qemu_pid = get_qemu_pid();
    
    if (global_qemu_pid <= 0) {
        fprintf(stderr, "Error! QEMU process not find, verify if the VM is active.\n");
        return -1;
    }
    
    printf("VM Detected with the PID: %d\n", global_qemu_pid);
    
    // Handles the "CTRL+C" signal, When closed it will restore to default automatically
    signal(SIGINT, restore_system);
    
    // Changing the kernel aggressiveness
    write_sys_file("/proc/sys/vm/compaction_proactiveness", "100");
    
    printf("Cleaning the cache and soliciting the 16GB of Hugepages...\n");
    write_sys_file("/proc/sys/vm/drop_caches", "3");
    
    write_sys_file("/proc/sys/vm/compact_memory", "1");

    sleep(2);
    
    write_sys_file("/proc/sys/vm/nr_hugepages", "6144");
    
    FILE *f = fopen("/proc/sys/vm/nr_hugepages", "r");
    int allocated = 0;
    fscanf(f, "%d", &allocated);
    fclose(f);
    
    if (allocated < 6144) {
        fprintf(stderr, "Allocation incomplete: Only got %d pages.\n", allocated);
        restore_system(0);
        return -1;
    }
    
    // Apply the Real-Time priority
    struct sched_param param;
    param.sched_priority = 99;
    
    if (sched_setscheduler(global_qemu_pid, SCHED_FIFO, &param) == -1) {
        fprintf(stderr, "ERROR! Run with sudo! (Permission Denied for SHCED_FIFO)\n");
        restore_system(0);
        return -1;
    }
    
    // Isolate all the IRQs and E-cores (CPUs 4-11 -> mask f0)
    write_sys_file("/proc/irq/default_smp_affinity", "f0");
    
    
    set_conio_terminal_mode();
    
    printf("Optimizations sucessfully applied!\n");
    printf("Press 'q' or 'z' to exit and restore to default.\n");
    
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    
    while (1) {
        // Verify if any key is pressed (timeout of 500ms)
        if (poll(fds, 1, 500) > 0) {
            char c;
            read(STDIN_FILENO, &c, 1);
            if (c == 'q' || c =='Q' || c == 'z' || c == 'Z') {
                restore_system(0);
            }
        }
        
        // Verify if the QEMU is still active
        if (get_qemu_pid() <= 0) {
            printf("\n VM closed detected, Cleaning...\n");
            restore_system(0);
        }
    }
    
    while (1) { pause(); }
    
    return 0;
}
