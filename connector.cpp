// Simple console message communicator
// Single binary can act as server (listen) or client (connect)
// Uses threads so reading stdin and receiving from socket don't block each other.

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <cerrno>
#include <csignal>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

static atomic<bool> running{true};

int create_server_socket(const string &port_str) {
  struct addrinfo hints{}, *res;
  hints.ai_family = AF_INET; // IPv4
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int ret = getaddrinfo(NULL, port_str.c_str(), &hints, &res);
  if (ret != 0) {
    cerr << "getaddrinfo: " << gai_strerror(ret) << endl;
    return -1;
  }

  int sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sfd < 0) {
    perror("socket");
    freeaddrinfo(res);
    return -1;
  }

  int opt = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(sfd, res->ai_addr, res->ai_addrlen) != 0) {
    perror("bind");
    close(sfd);
    freeaddrinfo(res);
    return -1;
  }

  freeaddrinfo(res);

  if (listen(sfd, 1) != 0) {
    perror("listen");
    close(sfd);
    return -1;
  }
  return sfd;
}

int create_client_socket(const string &host, const string &port_str) {
  struct addrinfo hints{}, *res, *rp;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
  if (ret != 0) {
    cerr << "getaddrinfo: " << gai_strerror(ret) << endl;
    return -1;
  }

  int sfd = -1;
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) continue;
    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
    close(sfd);
    sfd = -1;
  }
  freeaddrinfo(res);
  if (sfd == -1) {
    perror("connect");
  }
  return sfd;
}

void recv_loop(int sockfd) {
  const size_t BUF = 1024;
  vector<char> buf(BUF);
  while (running.load()) {
    ssize_t n = recv(sockfd, buf.data(), BUF - 1, 0);
    if (n > 0) {
      buf[n] = '\0';
      cout << "[remote] " << buf.data();
      if (buf[n-1] != '\n') cout << '\n';
      cout.flush();
    } else if (n == 0) {
      cerr << "Connection closed by peer" << endl;
      running.store(false);
      break;
    } else {
      if (errno == EINTR) continue;
      perror("recv");
      running.store(false);
      break;
    }
  }
}

void send_loop(int sockfd, string username) {
  string line;
  while (running.load() && std::getline(cin, line)) {
    // Prefix the outgoing message with the username so the peer can see who sent it.
    string out = username + ": " + line + "\n";
    // Optional local echo of sent message
    cout << "[you] " << username << ": " << line << endl;

    ssize_t sent = 0;
    const char *data = out.data();
    size_t to_send = out.size();
    while (to_send > 0) {
      ssize_t n = send(sockfd, data + sent, to_send, 0);
      if (n < 0) {
        if (errno == EINTR) continue;
        perror("send");
        running.store(false);
        return;
      }
      sent += n;
      to_send -= n;
    }
  }
  // EOF on stdin or error -> stop
  running.store(false);
}

void handle_sigint(int) {
  running.store(false);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cerr << "Usage:\n  As server: " << argv[0] << " --listen <port>\n  As client: " << argv[0] << " <host> <port>\n";
    return 1;
  }

  signal(SIGINT, handle_sigint);

  int conn_fd = -1;

  string username;
  if (string(argv[1]) == "--listen") {
    if (argc < 3) {
      cerr << "Please specify port to listen on\n";
      return 1;
    }
    string port = argv[2];
    // optional username as third arg
    if (argc >= 4) username = argv[3]; else username = "server";

    int srv = create_server_socket(port);
    if (srv < 0) return 1;
    cout << "Listening on port " << port << " ... waiting for a connection" << endl;
    struct sockaddr_storage peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    conn_fd = accept(srv, (struct sockaddr*)&peer_addr, &peer_len);
    if (conn_fd < 0) {
      perror("accept");
      close(srv);
      return 1;
    }
    close(srv); // single connection server
    cout << "Client connected" << endl;
  } else {
    if (argc < 3) {
      cerr << "Please specify host and port\n";
      return 1;
    }
    string host = argv[1];
    string port = argv[2];
    // optional username as third arg
    if (argc >= 4) username = argv[3]; else username = "host";

    conn_fd = create_client_socket(host, port);
    if (conn_fd < 0) return 1;
    cout << "Connected to " << host << ":" << port << endl;
  }

  thread r(recv_loop, conn_fd);
  thread s(send_loop, conn_fd, username);

  // Wait for either thread to finish (they update running flag)
  while (running.load()) this_thread::sleep_for(chrono::milliseconds(100));

  // ensure threads stop and socket closes
  shutdown(conn_fd, SHUT_RDWR);
  close(conn_fd);

  if (s.joinable()) s.join();
  if (r.joinable()) r.join();

  cout << "Exiting." << endl;
  return 0;
}
