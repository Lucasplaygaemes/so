# so - System Orchestrator

**so** or (**SO**) is a system utility designed to orchestrate Linux kernel resources for low-latency virtualization (KVM/QEMU).   

## Features

- **Persistent Configuration**: Saves your preferences (RAM percentage and Hugepages count) to so.conf so you don't have to re-configure on every run.

- **Auto-Discovery & Spec Detection**:  Automatically detects total system RAM and available CPU threads to calculate optimal resource allocation.

- **Memory Management**: Automated allocation of 2MB Hugepages with pro-active memory compaction to prevent fragmentation.

- **CPU Pinning & Scheduling**: Dynamically sets QEMU process priority to SCHED_FIFO (Real-Time) with max priority (99).

- **Interrupt Isolation**: Re-routes IRQ affinity to specific cores to reduce jitter on P-Cores.

- **Clean Restoration**: Implements a failsafe exit (Keyboard/Signal) to restore system state and return RAM to the host.

## Architecture
The orchestrator follows a strict resource-lifecycle:

1. **Setup**: Checks for so.conf. If missing, it enters Discovery Phase to detect hardware and ask for user preferences.

2. **Detection**: Waits for the QEMU process to spawn.

3. **Optimization**: Triggers `drop_caches` and `compact_memory` before requesting hugepages.

4. **Isolation**: Uses bitmasks to separate E-cores from P-cores for IRQ processing.

5. **Monitoring**: Uses a poll() loop to monitor user input ('q', 'z', or 'c' to exit) and detects if the VM is closed to trigger auto-restoration.

## Usage

```bash
# Compile
gcc so.c -o so

# Run (Requires sudo for /proc and scheduling access)
sudo ./so
```

# Objectives
The objectives is to increase the speed and resources available to the VM, The code IS hard-coded a CPU preset, so it should be changed to make it compatible with other architectures.

# Technical Notes
- **CPU Architecture**: The IRQ affinity is currently hard-codded with the mask, (e.g:, f0). While the program detects the number of threads, you should verify the bitmask in ```main()``` to match your specific P-Core/E-Core layout.

# TODO
The next step is to add a way to automatically divide the resources, like between E-Cores and P-CORES, to match the machine cores layout. 
