# Connector — simple console message communicator

This is a tiny single-file C++ program that can act either as a server (listen) or a client (connect). It uses threads so reading stdin and receiving socket data don't block each other.

Build

```bash
g++ -std=c++11 -pthread connector.cpp -o connector
```

Usage


Server:

```bash
./connector --listen 12345 [name]
```

Client:

```bash
./connector 127.0.0.1 12345 [name]
```

Type messages and press Enter to send. Sent messages are prefixed with your chosen name and received messages are printed with a `[remote]` prefix.

If you omit [name], the server defaults to `server` and the client defaults to `host`.

To quit: Ctrl+D (EOF on stdin) or Ctrl+C.

Notes

- Single connection at a time (keeps implementation simple).
- Works on Linux / POSIX systems.
