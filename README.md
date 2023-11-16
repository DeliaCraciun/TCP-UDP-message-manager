# TCP-UDP-message-manager
# TCP and UDP Client-Server Application for Message Management

## Overview
Developed a comprehensive client-server application for efficient message management using TCP and UDP protocols.


## Key Features
- **TCP Message Protocol**: Structure containing `<ID_CLIENT>`, message buffer, and message length.
- **UDP Message Protocol**: Structure with topic buffer, content buffer, and content type.

## TCP Client
- Establishes connection with the server.
- Sends client ID upon initialization.
- Writes to and receives messages from the server.

## Server Functionality
- **Message Conversion**: Implements `convert_udp_message()` for processing UDP messages based on data type.
- **Socket Management**: Utilizes poll for IO multiplexing, handling TCP/UDP sockets and server exit commands.
- **Connection Handling**: Manages new TCP connections and checks for existing clients.
- **Topic Management**: Handles UDP packets for topic updates, broadcasting to subscribed TCP clients.

## Build and Run

### Requirements
- GCC Compiler
- Make

### Compiling the Application
To compile the server and subscriber, run the following command in the terminal:

```bash
make all
