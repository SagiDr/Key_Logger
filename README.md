# Linux Stealth Keylogger & Kernel-to-User Architecture

A Linux systems programming project combining kernel-space hardware interrupt handling, asynchronous communication, and system-wide stealth and persistence mechanisms.

---

## Features

- **Kernel Module (LKM):** Captures keyboard events at the IRQ level, translates scancodes, and processes keystrokes inside kernel space.
- **Netlink Sockets:** Provides fast, asynchronous communication between kernel space and user space without legacy polling.
- **User-Space Stealth (`LD_PRELOAD`):** Implements function hooking (`readdir`, `fgets`, `read`) to hide files, processes, and kernel modules from common utilities such as `ls`, `ps`, and `cat`.
- **Automated Deployment (`setup.sh`):** Automates compilation, module loading, device creation, Systemd persistence, and command wrapper installation.
- **Log Analysis Tool (`search_logs.py`):** Searches collected logs for keywords and cleans formatting tags.

---

## Project Structure

```text
.
├── Makefile                 # Builds the kernel module and user-space programs
├── keylogger.c              # Kernel module (LKM)
├── client.c                 # User-space daemon receiving Netlink events
├── client.h                 # Shared client definitions
├── server.c                 # Remote log collection server
├── stealth_hook.c           # LD_PRELOAD stealth library
├── setup.sh                 # Automated installation and persistence
└── search_logs.py           # Log search and cleanup utility
```

---

## Building the Project

Compile the kernel module and all user-space binaries:

```bash
make
```

---

## Installation

Run the deployment script with root privileges:

```bash
sudo ./setup.sh
```

The installation script automatically:

- Compiles the project (if needed)
- Loads the kernel module
- Creates the required device files
- Registers persistent Systemd services
- Installs the stealth components
- Configures command wrappers

---

## Log Analysis

Search collected log files for a specific keyword:

```bash
python3 search_logs.py <log_file> <keyword>
```

Example:

```bash
python3 search_logs.py logs.txt password
```

---

## Remote Collection Server

On the central management machine (the server responsible for receiving the collected logs), compile and start the server:

```bash
gcc server.c -o server
./server <port>
```

Example:

```bash
gcc server.c -o server
./server 5555
```

---

## Components Overview

| Component | Description |
|-----------|-------------|
| **Kernel Module** | Captures keyboard input and forwards events through Netlink. |
| **Client Daemon** | Receives kernel events, manages logging, and forwards data when required. |
| **Server** | Accepts incoming log data from remote clients. |
| **Stealth Library** | Uses `LD_PRELOAD` hooks to conceal files, processes, and kernel modules. |
| **Setup Script** | Automates installation, persistence, and environment configuration. |
| **Search Utility** | Filters and cleans collected log files for easier analysis. |
