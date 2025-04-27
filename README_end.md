# Chat Application

This is a modular chat application that supports both server-based communication and peer-to-peer (P2P) connections. The application is built using `C` and uses `ncurses` for the user interface, along with socket programming for networking.

---

## Features

- **Real-time Chat**: Communicate with other users via a central server.
- **Peer-to-Peer Communication**: Establish direct connections with peers for private chats.
- **Chat Logging**: Save and view chat history with specific users.
- **Command Support**:
  - `/quit`: Exit the application.
  - `/p2p <peer_ip>:<peer_port>`: Connect to a peer for direct communication.
  - `/log <target_user>`: View chat history with a specific user.

---

## Project Structure

The project is organized into multiple modules for better readability and maintainability:

### 1. **Main File**
- **`client.c`**: The entry point of the application. It integrates all modules and handles the main chat loop.

### 2. **Modules**
- **`Utils/cJSON.c`**:
  - Handles JSON parsing and serialization for chat logging.

- **`client_p2p.c`**:
  - Manages peer-to-peer communication.
  - Functions:
    - `start_p2p_server()`: Starts a P2P server to accept connections.
    - `connect_to_p2p_peer(const char *peer_ip, int peer_port)`: Connects to a P2P peer.
    - `send_p2p_message(const char *message)`: Sends a message to a P2P peer.
    - `receive_p2p_message()`: Receives a message from a P2P peer.
    - `close_p2p_connection()`: Closes the P2P connection.

- **`server.c`**:
  - Handles server-side operations for managing multiple clients.

### 3. **Headers**
- **`client.h`**:
  - Contains shared constants, structures, and function declarations.
- **`client_p2p.h`**:
  - Contains declarations for P2P-related functions.
- **`server.h`**:
  - Contains declarations for server-related functions.
- **`Utils/cJSON.h`**:
  - Contains declarations for JSON-related functions.

---

## How to Build and Run

### Prerequisites
- GCC compiler
- `ncurses` library installed on your system

### Build Instructions
1. Navigate to the project directory:
   ```bash
   cd "d:\OS Project 2025\OS-Project2025"
   ```
2. Compile the project using the `makefile`:
   ```bash
   make
   ```
3. Run the client application:
   ```bash
   ./client.exe
   ```
4. Run the server application (if needed):
   ```bash
   ./server.exe
   ```

---

## Usage

1. **Start the Application**:
   - Run the compiled binary.
   - Enter your username when prompted.

2. **Commands**:
   - `/quit`: Exit the application.
   - `/p2p <peer_ip>:<peer_port>`: Connect to a peer for direct communication.
   - `/log <target_user>`: View chat history with a specific user.

3. **Chat**:
   - Type messages and press `Enter` to send them to the server or the current P2P connection.

---

## File Structure

```
OS-Project2025/
├── client.c               # Main entry point
├── client_p2p.c           # P2P communication functionality
├── server.c               # Server-side functionality
├── Utils/cJSON.c          # JSON parsing and serialization
├── client.h               # Shared constants and declarations
├── client_p2p.h           # P2P-related declarations
├── server.h               # Server-related declarations
├── Utils/cJSON.h          # JSON-related declarations
├── makefile               # Build instructions
├── README.md              # Project documentation
├── README_end.md          # Extended project documentation
└── Logs/                  # Chat logs
    ├── Sh.json
    └── Shaheer.json
```

---

## Results

### Expected Outcomes

1. **Server Connection**:
   - The application should successfully connect to the central server.
   - A message confirming the connection should appear in the chat window.

2. **Real-time Chat**:
   - Users should be able to send and receive messages in real-time.
   - Messages should appear in the chat window with proper formatting.

3. **Peer-to-Peer Communication**:
   - Users should be able to establish direct connections with peers using the `/p2p` command.
   - Messages sent over P2P should bypass the server and appear directly in the peer's chat window.

4. **Chat Logging**:
   - Messages should be logged in a JSON file for each user.
   - Users should be able to view their chat history using the `/log` command.

5. **User Interface**:
   - The application should display a clean and responsive UI with separate windows for:
     - Chat messages
     - Online users
     - Input area
     - Title bar

6. **Command Handling**:
   - The application should handle commands like `/quit`, `/p2p`, and `/log` gracefully.
   - Invalid commands should display an error message.

---

## Diagram of Results Flow

Below is a diagram representing the flow of the application and its results:

```
+-------------------+
|    Start App      |
+-------------------+
          |
          v
+-------------------+       +-------------------+
| Connect to Server |       |  P2P Connection   |
| (Server Messages) |<----->| (Direct Messages) |
+-------------------+       +-------------------+
          |
          v
+-------------------+
|  Chat Logging     |
| (Save & View Logs)|
+-------------------+
          |
          v
+-------------------+
| User Interface    |
| (Responsive UI)   |
+-------------------+
```

---

## Summary of Results

| Test Case                  | Expected Result | Actual Result | Status  |
|----------------------------|-----------------|---------------|---------|
| Server Connection          | ✅ Success      | ✅ Success    | Passed  |
| Send and Receive Messages  | ✅ Success      | ✅ Success    | Passed  |
| Peer-to-Peer Communication | ✅ Success      | ✅ Success    | Passed  |
| Chat Logging               | ✅ Success      | ✅ Success    | Passed  |
| User Interface             | ✅ Success      | ✅ Success    | Passed  |
| Command Handling           | ✅ Success      | ✅ Success    | Passed  |

---

## Known Issues

- **P2P Connection Timeout**:
  - If the peer does not respond within a certain time, the connection attempt may hang.
  - **Workaround**: Implement a timeout for P2P connections.

- **Large Chat Logs**:
  - Viewing large chat logs may cause a delay in rendering.
  - **Workaround**: Paginate the chat history.

---

## Future Enhancements

- Add encryption for P2P communication.
- Implement file sharing between peers.
- Improve error handling for invalid commands and connection failures.

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.