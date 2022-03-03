#pragma once

#include <stdexcept>

namespace exmdbpp
{

/**
 * @brief      Base class for exceptions thrown by exmdbpp
 */
class ExmdbError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

/**
 * @brief   Exception thrown on connection errors
 */
class ConnectionError : public ExmdbError
{
public:
	using ExmdbError::ExmdbError;
};

/**
 * @brief   Exception thrown when exmdb server returns an error code
 */
class ExmdbProtocolError : public ExmdbError
{
public:
	ExmdbProtocolError(const std::string&, uint8_t);

	const uint8_t code; ///< Response code returned by the server

	static std::string description(uint8_t);
};

/**
 * @brief   Exception thrown when serialization or deserialization fails
 */
class SerializationError : public ExmdbError
{
public:
	using ExmdbError::ExmdbError;
};

}
