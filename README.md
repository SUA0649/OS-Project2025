# NERV Terminal - Multi-threaded Chat System

## Project Description
A real-time chat application implementing a client-server architecture with multi-threading and synchronization mechanisms in C. The system enables secure communication between multiple clients through a centralized server.

## Key Features
- **Multi-client Support**: Handles concurrent connections using POSIX threads
- **Message Types**:
  - Broadcast messages to all connected clients
  - Private messages between specific users
- **Synchronization**:
  - Mutex-protected shared resources
  - Condition variables for message queue signaling
- **Optional GUI**: Terminal interface using NCurses or GTK

## Technical Stack
| Component              | Technology               |
|------------------------|--------------------------|
| Language               | C                        |
| Networking             | POSIX Socket Programming |
| Threading              | Pthread Library          |
| Synchronization        | Mutexes & Condition Variables |
| UI Framework           | NCurses/GTK (Optional)   |
| Development Environment| Linux (Ubuntu)           |

## System Architecture
```mermaid
flowchart TD
    Client1 -->|Socket| Server
    Client2 -->|Socket| Server
    Server -->|Broadcast| Client1
    Server -->|Private| Client2
    Server -->|Thread| ThreadPool
