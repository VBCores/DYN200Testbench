#pragma once

#include <stdexcept>
#include <string>

namespace voltbro::testbench {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what) : std::runtime_error(what) {}
};

class SocketCanError : public Error {
public:
    explicit SocketCanError(const std::string& what) : Error(what) {}
};

class ProtocolError : public Error {
public:
    explicit ProtocolError(const std::string& what) : Error(what) {}
};

}  // namespace voltbro::testbench
