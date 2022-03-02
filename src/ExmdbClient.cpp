/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2020-2021 grommunio GmbH
 */
#include "ExmdbClient.h"
#include "IOBufferImpl.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "constants.h"

using namespace std::string_literals;

namespace exmdbpp
{

using namespace requests;
using namespace constants;

/**
 * @brief      Constructor
 *
 * @param      message  Error message
 * @param      code     Exmdb response code
 */
ExmdbError::ExmdbError(const char *message, uint8_t code) :
	std::runtime_error(message + " ("s + std::to_string(code) + ")"), code(code)
{}
ExmdbError::ExmdbError(std::string &&message, uint8_t code) :
	std::runtime_error(std::move(message) + " ("s + std::to_string(code) + ")"), code(code)
{}

///////////////////////////////////////////////////////////////////////////////////////////////////


static const addrinfo aiHint = {
    0, //ai_flags
    AF_UNSPEC, //ai_family
    SOCK_STREAM, //ai_socktype
    0, //ai_protocol
    0, nullptr, nullptr, nullptr
};

/**
 * @brief      Destructor
 *
 * Automatically closes the socket if it is still open.
 */
ExmdbClient::Connection::~Connection()
{close();}

/**
 * @brief      Move constuctor
 *
 * @param      Connection to move from
 */
ExmdbClient::Connection::Connection(ExmdbClient::Connection&& other) noexcept : sock(other.sock)
{other.sock = -1;}

/**
 * @brief      Move assignment operator
 *
 * @param      Connection to move from
 */
ExmdbClient::Connection& ExmdbClient::Connection::operator=(ExmdbClient::Connection&& other) noexcept
{
	if(this == &other)
		return *this;
	close();
	sock = other.sock;
	other.sock = -1;
	return *this;
}

/**
 * @brief      Close the socket
 *
 * Has no effect when the socket is already closed.
 */
void ExmdbClient::Connection::close()
{
	if(sock != -1)
		::close(sock);
	sock = -1;
}

/**
 * @brief      Connect to server
 *
 * Establishes a TCP connection to the specified server.
 *
 * @param      host  Server address
 * @param      port  Server port or service
 *
 * @throws     std::runtime_error Connection could not be established
 */
void ExmdbClient::Connection::connect(const std::string& host, const std::string& port)
{
	if(sock != -1)
		close();
	addrinfo* addrs;
	int error;
	if((error = getaddrinfo(host.c_str(), port.c_str(), &aiHint, &addrs)))
		throw std::runtime_error("Could not resolve address: "+std::string(gai_strerror(error)));
	for(addrinfo* addr = addrs; addr != nullptr; addr = addr->ai_next)
	{
		if((sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol))  == -1)
			continue;
		if(::connect(sock, addr->ai_addr, addr->ai_addrlen) == 0)
			break;
		error = errno;
		::close(sock);
		sock = -1;
	}
	freeaddrinfo(addrs);
	if(sock == -1)
		throw std::runtime_error("Connect failed: "+std::string(strerror(error)));
}

static const char *response_to_str(uint8_t status)
{
	switch (status) {
	case ResponseCode::SUCCESS: return nullptr;
	case ResponseCode::ACCESS_DENY: return "Access denied";
	case ResponseCode::MAX_REACHED: return "Server reached maximum number of connections";
	case ResponseCode::LACK_MEMORY: return "Out of memory";
	case ResponseCode::MISCONFIG_PREFIX: return "Prefix not served";
	case ResponseCode::MISCONFIG_MODE: return "Prefix has type mismatch";
	case ResponseCode::CONNECT_INCOMPLETE: return "No prior CONNECT RPC made";
	case ResponseCode::PULL_ERROR: return "Invalid request/Server-side deserializing error";
	case ResponseCode::DISPATCH_ERROR: return "Dispatch error";
	case ResponseCode::PUSH_ERROR: return "Server-side serialize error";
	default: return "Unknown error";
	}
}

/**
 * @brief      Send request
 *
 * Sends data contained in the buffer to the server.
 *
 * The response code and length are inspected and the response (excluding status code and length)
 * is written back into the buffer.
 *
 * @param      buff  Buffer used for sending and receiving data
 */
void ExmdbClient::Connection::send(IOBuffer& buff)
{
	ssize_t bytes = ::send(sock, buff.data(), buff.size(), MSG_NOSIGNAL);
	if(bytes < 0)
		throw std::runtime_error("Send failed: "+std::string(strerror(errno)));
	buff.resize(5);
	bytes = recv(sock, buff.data(), 5, 0);
	if(bytes < 0)
		throw std::runtime_error("Receive failed: "+std::string(strerror(errno)));
	uint8_t status = buff.pop<uint8_t>();
	auto e = response_to_str(status);
	if(e != nullptr)
		throw ExmdbError("Server returned non-zero response code: "s + e, status);
	if(bytes < 5)
		throw std::runtime_error("Connection closed unexpectedly");
	uint32_t length = buff.pop<uint32_t>();
	buff.reset();
	buff.resize(length);
	for(uint32_t offset = 0;offset < length;offset += bytes)
	{
		bytes = recv(sock, buff.data()+offset, length-offset, 0);
		if(bytes < 0)
			throw std::runtime_error("Message reception failed");
		if(bytes == 0)
			throw std::runtime_error("Connection closed unexpectedly");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Initialize client and connect to server
 *
 * @param      host       Server adress
 * @param      port       Server port
 * @param      prefix     Data area prefix (passed to ConnectRecquest)
 * @param      isPrivate  Whether to access private or public data (passed to ConnectRequest)
 */
ExmdbClient::ExmdbClient(const std::string& host, const std::string& port, const std::string& prefix, bool isPrivate)
{connect(host, port, prefix, isPrivate);}

/**
 * @brief      Connect to server
 *
 * @param      host       Server adress
 * @param      port       Server port
 * @param      prefix     Data area prefix (passed to ConnectRecquest)
 * @param      isPrivate  Whether to access private or public data (passed to ConnectRequest)
 */
void ExmdbClient::connect(const std::string& host, const std::string& port, const std::string& prefix, bool isPrivate)
{
	connection.connect(host, port);
	send<ConnectRequest>(prefix, isPrivate);
}

}
