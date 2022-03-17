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
#include <fcntl.h>
#include <poll.h>

#include <stdexcept>

#include "constants.h"

namespace exmdbpp
{

using namespace requests;
using namespace constants;


const uint8_t ExmdbClient::AUTO_RECONNECT = 1<<0; ///< Automatically reconnect on dispatch error


/**
 * @brief      Constructor
 *
 * @param      message  Error message
 * @param      code     Exmdb response code
 */
ExmdbProtocolError::ExmdbProtocolError(const std::string& message, uint8_t code) :
    ExmdbError(message+description(code)), code(code)
{}

std::string ExmdbProtocolError::description(uint8_t code)
{
	switch(code)
	{
	case ResponseCode::SUCCESS: return "Success.";
	case ResponseCode::ACCESS_DENY: return "Access denied";
	case ResponseCode::MAX_REACHED: return "Server reached maximum number of connections";
	case ResponseCode::LACK_MEMORY: return "Out of memory";
	case ResponseCode::MISCONFIG_PREFIX: return "Prefix not served";
	case ResponseCode::MISCONFIG_MODE: return "Prefix has type mismatch";
	case ResponseCode::CONNECT_INCOMPLETE: return "No prior CONNECT RPC made";
	case ResponseCode::PULL_ERROR: return "Invalid request/Server-side deserializing error";
	case ResponseCode::DISPATCH_ERROR: return "Dispatch error";
	case ResponseCode::PUSH_ERROR: return "Server-side serialize error";
	default: return "Unknown error code "+std::to_string(code);
	}
}

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
 * @throws     ConnectionError   Connection could not be established
 */
void ExmdbClient::Connection::connect(const std::string& host, const std::string& port)
{
	if(sock != -1)
		close();
	addrinfo* addrs;
	pollfd fd;
	fd.events = POLLOUT;
	int error, res = 0;
	if((error = getaddrinfo(host.c_str(), port.c_str(), &aiHint, &addrs)))
		throw ConnectionError("Could not resolve address: "+std::string(gai_strerror(error)));
	for(addrinfo* addr = addrs; addr != nullptr; addr = addr->ai_next)
	{
		if((sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol))  == -1)
			continue;
		int flags = fcntl(sock, F_GETFL);
		if(flags < 0)
		{
			error = errno;
			res = -1;
			continue;
		}
		fcntl(sock, F_SETFL, flags | O_NONBLOCK);
		::connect(sock, addr->ai_addr, addr->ai_addrlen);
		fd.fd = sock;
		res = poll(&fd, 1, 3000);
		if(res == 1)
			break;
		error = errno;
		::close(sock);
		sock = -1;
	}
	freeaddrinfo(addrs);
	if(res == 0)
		throw ConnectionError("Connect failed: connection timeout");
	if(res < 0)
		throw ConnectionError("Connect failed: "+std::string(strerror(error)));
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) & ~O_NONBLOCK);
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
		throw ConnectionError("Send failed: "+std::string(strerror(errno)));
	buff.resize(5);
	bytes = recv(sock, buff.data(), 5, 0);
	if(bytes < 0)
		throw ConnectionError("Receive failed: "+std::string(strerror(errno)));
	else if(bytes == 0)
		throw ConnectionError("Connection closed unexpectedly");
	uint8_t status = buff.pop<uint8_t>();
	if(status != ResponseCode::SUCCESS)
		throw ExmdbProtocolError("exmdb call failed: ", status);
	if(bytes < 5)
		throw ConnectionError("Short read");
	uint32_t length = buff.pop<uint32_t>();
	buff.reset();
	buff.resize(length);
	for(uint32_t offset = 0;offset < length;offset += bytes)
	{
		bytes = recv(sock, buff.data()+offset, length-offset, 0);
		if(bytes < 0)
			throw ConnectionError("Message reception failed");
		if(bytes == 0)
			throw ConnectionError("Connection closed unexpectedly");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ExmdbClient::ConnParm::ConnParm(const std::string& host, const std::string& port, const std::string& prefix, bool isPrivate) :
    host(host), port(port), prefix(prefix), isPrivate(isPrivate)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief      Initialize client and connect to server
 *
 * @param      host       Server adress
 * @param      port       Server port
 * @param      prefix     Data area prefix (passed to ConnectRecquest)
 * @param      isPrivate  Whether to access private or public data (passed to ConnectRequest)
 */
ExmdbClient::ExmdbClient(const std::string& host, const std::string& port, const std::string& prefix, bool isPrivate,
                         uint8_t flags) : params(host, port, prefix, isPrivate), flags(flags)
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
	params = ConnParm(host, port, prefix, isPrivate);
	connection.connect(host, port);
	send<ConnectRequest>(prefix, isPrivate);
}

/**
 * @brief      Reconnect to the exmdb server
 *
 * If reconnect fails, the current connection is left intact.
 *
 * @return     true if successful, false otherwise
 */
bool ExmdbClient::reconnect()
{
	Connection newconn;
	try
	{
		newconn.connect(params.host, params.port);
		buffer.clear();
		buffer.start();
		ConnectRequest::write(buffer, params.prefix, params.isPrivate);
		buffer.finalize();
		newconn.send(buffer);
		connection = std::move(newconn);
		return true;
	}
	catch (...)
	{return false;}
}

}
