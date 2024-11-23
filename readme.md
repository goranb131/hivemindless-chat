compile client on client connecting to server:
clang -D_POSIX_C_SOURCE=200809L -o chat_client client.c -lpthread


compile server where chat is served from and server user:

clang -o chat_server server.c -lpthread


TalkFree Chat: No Bloat. Just Talk.

TalkFree is a minimalist terminal-based chat application built in C, designed to run on UNIX systems like FreeBSD, macOS, and Illumos. It focuses on simplicity, privacy, and efficiency, avoiding the unnecessary complexity of modern bloated communication tools.

Features

	•	Real-time messaging via TCP sockets
	•	Unique nicknames for each user
	•	Multi-client support with broadcast messaging
	•	Simple and elegant terminal-based interface
	•	ANSI color codes for better readability

Usage

Server

	•	Run the chat_server on a UNIX system to start a chat server.
	•	The server listens on port 12345 by default and supports multiple clients.

Client

	•	Run the chat_client on any UNIX system to connect to the server.
	•	Clients can connect from the same machine or over a network, using the server’s IP address.

Example Setup

	1.	Start the server:
./chat_server
	2.	Connect a client:
./chat_client
	3.	For multiple clients on the same machine, open another terminal and run:
./chat_client

Technical Overview

	•	Language: C, using POSIX APIs for maximum compatibility.
	•	Networking: Built on the Berkeley sockets API (socket, bind, connect, etc.).
	•	Concurrency: Uses pthreads to handle multiple clients simultaneously.
	•	Synchronization: Mutexes ensure thread-safe operations on shared resources.
	•	Terminal Interaction: Utilizes termios for raw input mode and clean line management.

User Details

	•	When the server starts, it waits for clients to connect and assigns them unique nicknames.
	•	Clients can send messages in real time, and all messages are broadcasted to connected users.
	•	If a nickname is already taken, the client is prompted to choose another.
	•	The server announces when users join or leave.

Compilation Guide

	1.	Ensure you have a C compiler (like clang or gcc) and make installed.
	2.	Compile the server and client manually:

	compile client on client connecting to server:
clang -D_POSIX_C_SOURCE=200809L -o chat_client client.c -lpthread


compile server where chat is served from and server user:

clang -o chat_server server.c -lpthread


3.	Run the compiled binaries from the terminal as described above.

Deployment Guide

	•	Place the chat_server binary on a server accessible over the network.
	•	Run the server:
./chat_server
	•	Place the chat_client binary on any client machine.
	•	Connect the client to the server by specifying the server’s IP address when prompted.

Place them in any location you wish, and run from there.

TODO

	•	Create a Makefile for easier compilation.
	•	Package chat_server and chat_client for FreeBSD ports, macOS .pkg, and other package managers.
	•	Add optional TLS support for secure communication.
	•	Optimize and test for large-scale usage across multiple clients.
	•	Explore persistent logging or optional message history.

Simple. Clean. No Bloat. Just Talk.