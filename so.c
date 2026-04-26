/*     THIS PROGRM IS MADE ONLY FOR EDUCATIONAL PURPOUSES 
       The objctive of this code is to make a program where
       Will change the priority of the program QEMU and it's 
       Process to take the best possible of the system on use 
       It also will restore, it's made with a specific
       processor architecture in mind, may work on others.      */

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
#include <sys/sysinfo.h>

#define CONFIG_FILE "so.conf"

int global_qemu_pid = 0;

struct termios old_t;

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
    
    // Restore the Compaction proactiveness to normal
    write_sys_file("/proc/sys/compaction_proactiveness", "20");
    
    // Retuns QEMU to the default priority
    if (global_qemu_pid > 0) {
        struct sched_param param;
        sched_setscheduler(global_qemu_pid, SCHED_OTHER, &param);
        printf("QEMU (PID %d) returned to the default escalator.\n", global_qemu_pid);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    
    printf("System clean, Exiting...\n");
    exit(0);
}


void auto_discover(float *ram_p, int *hugepages) {
    struct sysinfo si;
    sysinfo(&si);
    
    // Find the total RAM in the system
    long total_ram_mb = (si.totalram * si.mem_unit) / (1024 * 1024);
    // Find the number of cores in the system
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    printf("\n --- FASE OF DISCOVERY ---\n");
    printf("Hardware: %ld MB RAM | %d Threads detected.\n", total_ram_mb, cores);
    printf("Which porcantage of ram will be dedicated to the VM (Ex.: 0.5 for 50%%)? ");
    scanf("%f", ram_p);
    
    *hugepages = (int)(total_ram_mb * (*ram_p) / 2);
    
    FILE *f = fopen(CONFIG_FILE, "w");
    if (f) {
        fprintf(f, "RAM_PERCENT=%.2f\nTOTAL_HUGEPAGES=%d\n", *ram_p, *hugepages);
        fclose(f);
        printf("Configurations saved in %s\n", CONFIG_FILE);
    }
}

int load_config(float *ram_p, int *hugepages) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return 0;
    if (fscanf(f, "RAM_PERCENT=%f\nTOTAL_HUGEPAGES=%d\n", ram_p, hugepages) != 2) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int main(int argc, char *argv[]) {
    float ram_percent = 0;
    int target_hugepages = 0;
    int force_clean = (argc > 1 && strcmp(argv[1], "-c") == 0);
    
    if (!load_config(&ram_percent, &target_hugepages)) {
        auto_discover(&ram_percent, &target_hugepages);
    }
    
    signal(SIGINT, restore_system);
    printf("--- PREPARING THE AMBIENT FOR SO ---\n");
    // Variables to keep track of the memory and number of trys to allocate it
    int allocated = 0;
    int attempts = 0;
    char hp_val[16];
    snprintf(hp_val, 16, "%d", target_hugepages);
    
    while (allocated < target_hugepages && attempts < 3) {
        attempts++;
        printf("\r[Attempt %d] Compacting the RAM and requesting the Pages...", attempts);
        
        fflush(stdout);
        
        printf("Cleaning the cache and soliciting the %d MB of Hugepages...\n", allocated * 2);
        write_sys_file("/proc/sys/vm/drop_caches", "3");
        
        // Changhing the kernel's compact memory state
        write_sys_file("/proc/sys/vm/compact_memory", "1");
        
        // Setting the hugepages value  
        write_sys_file("/proc/sys/vm/nr_hugepages", hp_val);
        
        // Changing the kernel aggressiveness
        write_sys_file("/proc/sys/vm/compaction_proactiveness", "100");

        sleep(2);
        
        FILE *f = fopen("/proc/sys/vm/nr_hugepages", "r");
        if (f) {
            fscanf(f, "%d", &allocated);
            fclose(f);
        }
    }    
    
    // Verify if could allocate    
    if (allocated < target_hugepages) {
      fprintf(stderr, "[X] ERROR: Allocation incomplete: Only got %d pages.\n", allocated);
      restore_system(0);
      return -1;
    }
    printf("\n[✔] Success: %d MB reserved, now start the VM!\n", allocated * 2);

    set_conio_terminal_mode();
      
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
      
    // PID detection Loop      
    while ((global_qemu_pid = get_qemu_pid()) <= 0) {
    // Verify if any key is pressed (timeout of 500ms)
    if (poll(fds, 1, 100) > 0) {
        char c;
        read(STDIN_FILENO, &c, 1);
        if (c == 'q' || c =='Q' || c == 'z' || c == 'Z') {
              restore_system(0);
          }
          usleep(100000);
      }
  }
  printf("\n[+] VM detected (PID: %d). Applying prioritys...\n", global_qemu_pid);
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
  
  printf("Optimizations sucessfully applied!\n");
  printf("Press 'q' or 'z' to exit and restore to default.\n");
  
  while (1) {
      if (poll(fds, 1, 500) > 0) {
          char ch; read(STDIN_FILENO, &ch, 1);
          if (ch == 'q' || ch == 'c' || ch == 'Q' || ch == 'Z') restore_system(0);
          }
      // Verify if the QEMU is still active
      if (get_qemu_pid() <= 0) {
          printf("\n[!] VM closed detected, Cleaning...\n");
          restore_system(0);
      }
  }
  return 0;
}
