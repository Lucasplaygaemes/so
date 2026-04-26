# so - System Orchestrator

**so** or (**SO**) is a system utility designed to orchestrate Linux kernel resources for low-latency virtualization (KVM/QEMU).   

## Features
- **Memory Management**: Automated allocation of 2MB Hugepages with pro-active memory compaction to prevent fragmentation.

- **CPU Pinning & Scheduling**: Dynamically sets QEMU process priority to `SCHED_FIFO` (Real-Time).

- **Interrupt Isolation**: Re-routes IRQ affinity to specific cores to reduce jitter on P-Cores.

- **Clean Restoration**: Implements a failsafe exit (Keyboard/Signal) to restore system state and return RAM to the host.

## Architecture
The orchestrator follows a strict resource-lifecycle:
1. **Detection**: Waits for the QEMU process to spawn.

2. **Optimization**: Triggers `drop_caches` and `compact_memory` before requesting hugepages.

3. **Isolation**: Uses bitmasks to separate E-cores from P-cores for IRQ processing.

4. **Monitoring**: Uses a non-blocking `poll()` loop to monitor user input and VM status.

## Usage

```bash
# Compile
gcc so.c -o so

# Run (Requires sudo for /proc and scheduling access)
sudo ./so
```

# Objectives
The objectives of so is to increase the speed and resources available to the VM, The code IS hard-coded a CPU preset, so it should be changed to make it compatible with other architectures.

# TODO
Will make so be able to identify the specs of the local machine and if selected, automatically make the division of resources and priorities. 

