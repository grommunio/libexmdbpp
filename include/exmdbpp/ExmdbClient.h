#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include "exceptions.h"
#include "IOBuffer.h"
#include "requests.h"

/**
 * @brief Root namespace for the exmdbpp library
 */
namespace exmdbpp
{



/**
 * @brief   Client managing communication with the exmdb server
 */
class ExmdbClient
{
	class Connection
	{
	public:
		Connection() = default;
		~Connection();
		Connection(Connection&&) noexcept;
		Connection& operator=(Connection&&) noexcept;

		void connect(const std::string&, const std::string&);
		void close();
		void send(IOBuffer&);

	private:
		int sock = -1; ///< TCP socket to send and receive data
	};

	struct ConnParm
	{
		ConnParm() = default;
		ConnParm(const std::string&, const std::string&, const std::string&, bool);
		std::string host;
		std::string port;
		std::string prefix;
		bool isPrivate;
	};

public:
	ExmdbClient() = default;
	ExmdbClient(const std::string&, const std::string&, const std::string&, bool, uint8_t=0);

	void connect(const std::string&, const std::string&, const std::string&, bool);
	bool reconnect();

	template<class Request, typename... Args>
	requests::Response_t<Request> send(const Args&...);

	static const uint8_t AUTO_RECONNECT;
private:
	Connection connection; ///< Connection used to send and receive data
	ConnParm params; ///< Connection parameters
	IOBuffer buffer; ///< Buffer managing data to send / received data
	uint8_t flags = 0; ///< Client flags

};

/**
 * @brief      Send request and parse response
 * *
 * See documentation of the specific Request for a description of the
 * parameters.
 *
 * @param      args     Values to serialize
 *
 * @tparam     Request  Type of the request
 * @tparam     Args     Request arguments
 *
 * @return     Parsed response object
 */
template<class Request, typename... Args>
inline requests::Response_t<Request> ExmdbClient::send(const Args&... args)
{
	buffer.clear();
	buffer.start();
	Request::write(buffer, args...);
	buffer.finalize();
	try {connection.send(buffer);}
	catch (const ExmdbProtocolError& err)
	{
		if (err.code == 8 && flags & AUTO_RECONNECT)  // DISPATCH_ERROR
			reconnect();
		throw;
	}
	return requests::Response_t<Request>(buffer);
}


}

