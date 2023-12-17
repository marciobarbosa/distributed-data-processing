#include "network/Network.h"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

Network::Network()
    : sock(-1), port(-1), host(nullptr), server(nullptr), client(nullptr) {}

Network::Network(int port, Server *server)
    : sock(-1), port(port), host(nullptr), server(server), client(nullptr) {}

Network::Network(const char *host, int port, Client *client)
    : sock(-1), port(port), host(host), server(nullptr), client(client) {}

void Network::InitServer(std::shared_ptr<std::atomic<bool>> quit)
{
    this->quit = quit;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
	throw std::runtime_error("Error creating socket");
    }

    const int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
	throw std::runtime_error("Error setting SO_REUSEADDR");
    }

    sockaddr_in sip;
    sip.sin_family = AF_INET;
    sip.sin_addr.s_addr = INADDR_ANY;
    sip.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&sip), sizeof(sip)) == -1) {
	close(sock);
	throw std::runtime_error("Error binding socket");
    }
    if (listen(sock, 10) == -1) {
	close(sock);
	throw std::runtime_error("Error listening for connections");
    }

    rcv_t = std::thread(&Network::ReceiverServer, this);
    conn_t = std::thread(&Network::AcceptConnections, this);
    heartbeat_t = std::thread(&Network::HeartBeatsBroadcast, this);
}

void Network::InitClient(std::shared_ptr<std::atomic<bool>> quit)
{
    int status;
    auto sport = std::to_string(port);
    struct addrinfo hints, *result, *p;

    this->quit = quit;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(host, sport.c_str(), &hints, &result);
    if (status != 0) {
	throw std::runtime_error("Error resolving host");
    }

    for (p = result; p != nullptr; p = p->ai_next) {
	sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	if (sock == -1)
	    continue;
	if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
	    close(sock);
	    continue;
	}
	break;
    }
    freeaddrinfo(result);

    if (p == nullptr) {
	throw std::runtime_error("Error connecting to server");
    }
    ReceiverClient();
}

void Network::Serialize(const Packet& pkt, char *buffer, size_t bufsize)
{
    std::memcpy(buffer, &pkt, bufsize);
}

void Network::Deserialize(const char *buffer, size_t bufsize, Packet& pkt)
{
    std::memcpy(&pkt, buffer, bufsize);
}

void Network::ResolveServer(Packet& pkt, int socket)
{
    switch (pkt.opcode) {
    case RESULT:
	server->Request(socket, pkt.value);
	break;
    case HEARTBEAT:
	HeartBeatsRcv(socket);
	break;
    }
}

void Network::ResolveClient(Packet pkt)
{
    std::string message(pkt.message);
    switch (pkt.opcode) {
    case REQUEST:
	client->Request(message, pkt.value);
	break;
    case AGGREGATE:
	client->Aggregate(message, pkt.value);
	break;
    case HEARTBEAT:
	SendClient(HEARTBEAT, sock);
	break;
    }
}

bool Network::SendServer(int socket, int opcode, int value, std::string message)
{
    Packet pkt;
    ssize_t nbytes;
    char buffer[PKTSIZE];
    std::unique_lock<std::mutex> lock(mtx);

    std::memset(&pkt, 0, sizeof(pkt));
    std::memset(buffer, 0, PKTSIZE);

    pkt.opcode = opcode;
    pkt.value = value;
    message.copy(pkt.message, message.length());
    Serialize(pkt, buffer, PKTSIZE);

    // MSG_NOSIGNAL
    nbytes = send(socket, buffer, PKTSIZE, 0);
    if (nbytes == -1) {
        return false;
    }
    return true;
}

bool Network::SendClient(int opcode, int value)
{
    Packet pkt;
    ssize_t nbytes;
    char buffer[PKTSIZE];
    std::unique_lock<std::mutex> lock(mtx);

    std::memset(&pkt, 0, sizeof(pkt));
    std::memset(buffer, 0, PKTSIZE);

    pkt.opcode = opcode;
    pkt.value = value;
    Serialize(pkt, buffer, PKTSIZE);

    nbytes = send(sock, buffer, PKTSIZE, 0);
    if (nbytes == -1) {
        return false;
    }
    return true;
}

void Network::HeartBeatsRcv(int socket)
{
    std::unique_lock<std::mutex> lock(mtx);
    headbeats.erase(std::remove(headbeats.begin(), headbeats.end(), socket), headbeats.end());
}

void Network::HeartBeatsBroadcast()
{
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);

    while (!quit->load()) {
	lock.lock();
	headbeats = clients;
	lock.unlock();
	for (int client : headbeats) {
	    SendServer(client, HEARTBEAT, 0, "");
	}

	std::this_thread::sleep_for(std::chrono::seconds(HEARTBEATS_SECS));

	for (int client : headbeats) {
	    close(client);
	    lock.lock();
	    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
	    lock.unlock();
	    server->CrashedClient(client);
	}
    }
}

void Network::ReceiverServer()
{
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);

    while (!quit->load()) {
	Packet pkt;
	ssize_t nbytes;
	char buffer[PKTSIZE];
	int ready, maxsock = -1;

	std::memset(&pkt, 0, sizeof(pkt));
	std::memset(buffer, 0, PKTSIZE);

	fd_set sockset;
	FD_ZERO(&sockset);

	timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	lock.lock();
	for (int client : clients) {
	    if (client > maxsock)
		maxsock = client;
	    FD_SET(client, &sockset);
	}
	lock.unlock();

	ready = select(maxsock + 1, &sockset, nullptr, nullptr, &timeout);
	if (ready <= 0)
	    continue;

	lock.lock();
	for (int client : clients) {
	    if (FD_ISSET(client, &sockset) == 0)
		continue;
	    nbytes = recv(client, buffer, PKTSIZE, 0);
	    if (nbytes <= 0)
		continue;
	    Deserialize(buffer, nbytes, pkt);

	    lock.unlock();
	    ResolveServer(pkt, client);
	    lock.lock();
	}
	lock.unlock();
    }
}

void Network::ReceiverClient()
{
    while (!quit->load()) {
	Packet pkt;
	ssize_t nbytes;
	char buffer[PKTSIZE];

	std::memset(&pkt, 0, sizeof(pkt));
	std::memset(buffer, 0, PKTSIZE);

	nbytes = recv(sock, buffer, PKTSIZE, 0);
	if (nbytes <= 0) {
	    return;
	}
	Deserialize(buffer, nbytes, pkt);
	std::thread(&Network::ResolveClient, this, pkt).detach();
    }
}

void Network::AcceptConnections()
{
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);

    while (!quit->load()) {
	int ready;
	int cli_sock;
	sockaddr_in cip;
	socklen_t len = sizeof(cip);

	fd_set set;
	FD_ZERO(&set);
	FD_SET(sock, &set);

	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	ready = select(sock + 1, &set, nullptr, nullptr, &timeout);
	if (!(ready > 0 && FD_ISSET(sock, &set)))
	    continue;

	cli_sock = accept(sock, reinterpret_cast<struct sockaddr*>(&cip), &len);
	if (quit->load())
	    return;
	if (cli_sock == -1)
	    continue;
	lock.lock();
	clients.push_back(cli_sock);
	lock.unlock();
	// Pass this socket to the upper layer
	server->NewClient(cli_sock);
    }
}

void Network::TerminateServer()
{
    std::unique_lock<std::mutex> lock(mtx);
    for (int client : clients) {
	close(client);
    }
    close(sock);
    lock.unlock();

    heartbeat_t.join();
    rcv_t.join();
    conn_t.join();

    clients.clear();
}

void Network::TerminateClient()
{
    std::unique_lock<std::mutex> lock(mtx);
    for (int client : clients) {
	close(client);
    }
    close(sock);
    clients.clear();
}
