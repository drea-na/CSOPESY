# CSOPESY MO2 - Multitasking OS with Memory Management

## Overview
This is the enhanced version of the CSOPESY OS emulator with memory management capabilities, building upon MO1 features while adding demand paging, memory visualization, and backing store operations.

## New Features in MO2

### 1. Memory Management System
- **Demand Paging**: Pages are loaded into physical memory frames only when needed
- **Page Fault Handling**: Automatic handling when virtual memory pages are not in physical frames
- **Page Replacement**: FIFO algorithm when no free frames are available
- **Backing Store**: Swapped pages are saved to `csopesy-backing-store.txt`

### 2. New Commands

#### `process-smi`
Shows memory summary view (similar to nvidia-smi):
- Available/used memory
- Process list with memory usage
- Memory utilization percentage

#### `vmstat`
Shows detailed memory statistics:
- Total/used/free memory
- Page statistics (total frames, free frames, pages paged in/out)
- Process statistics (active/total processes)

### 3. Enhanced Screen Commands

#### `screen -s <process_name> [memory_size]`
Creates a process with optional memory allocation:
- Memory size must be power of 2 between 64-65536 bytes
- If not specified, uses minimum memory size from config

#### `screen -c <process_name> <memory_size> "<instructions>"`
Creates a process with custom instructions:
- Memory size must be power of 2 between 64-65536 bytes
- Instructions: 1-50 semicolon-separated commands
- Example: `screen -c test 512 "DECLARE varA 10; ADD varA varA varB; WRITE 0x500 varA"`

### 4. New Instructions

#### `READ(var, memory_address)`
Retrieves uint16 value from memory:
- Variables stored in 64-byte symbol table segment
- Maximum 32 variables per process
- Memory addresses in hexadecimal format
- Access violations shut down process with error message

#### `WRITE(memory_address, value)`
Writes uint16 value to memory address:
- Memory addresses in hexadecimal format
- Access violations shut down process with error message

### 5. Enhanced Error Handling
- Updated `screen -r` command shows memory access violations
- Format: "Process <name> shut down due to memory access violation error that occurred at <HH:MM:SS>. <Hex address> invalid."

## Configuration

### New config.txt Parameters
```
max-overall-mem 8192      # Maximum available memory (64-65536 bytes, power of 2)
mem-per-frame 512         # Memory size per frame/page (64-65536 bytes, power of 2)
min-mem-per-proc 64       # Minimum memory per process (64-65536 bytes, power of 2)
max-mem-per-proc 2048     # Maximum memory per process (64-65536 bytes, power of 2)
```

## Usage Examples

### Basic Memory Management
```
initialize
process-smi                    # View memory summary
vmstat                        # View detailed memory statistics
screen -s myprocess 1024      # Create process with 1KB memory
screen -c test 512 "DECLARE x 5; WRITE 0x100 x; READ y 0x100"
```

### Custom Instructions
```
screen -c calc 2048 "DECLARE a 10; DECLARE b 20; ADD result a b; PRINT result"
```

### Memory Access Simulation
```
screen -c memtest 1024 "WRITE 0x500 12345; READ value 0x500; PRINT value"
```

## File Structure
- `csopesy-backing-store.txt`: Backing store for swapped pages
- `process_logs/`: Process execution logs
- `csopesy-log.txt`: CPU utilization reports

## Technical Notes
- Memory addresses are emulated (not 1:1 physical RAM mapping)
- uint16 variables clamped between (0, max(uint16))
- Symbol table segment fixed at 64 bytes
- All memory ranges must be powers of 2
- Page fault handling integrated with scheduler
- Process isolation and memory protection enforced

## Building and Running
1. Open the project in Visual Studio
2. Build the solution (F7)
3. Run the executable
4. Type `initialize` to start
5. Use the new MO2 commands to test memory management features

## Assessment Requirements
- Black box quiz system under time pressure
- Parameter modification only (no recompilation during quiz)
- Video demonstrations (.MP4 format) required
- Some questions require PowerPoint presentations 