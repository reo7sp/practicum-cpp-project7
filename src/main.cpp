#include "headers.h"

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <cstdlib>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

class Handler {
public:
    boost::asio::awaitable<void> session(boost::asio::ip::tcp::socket client_socket) {
        try {
            co_await do_session(client_socket);
        } catch (const std::exception& e) {
            std::println(std::cerr, "{}", std::format("Session error: {}", e.what()));
        }

        co_return;
    }

private:
    static constexpr std::string_view headers_delimiter = "\r\n\r\n";

    boost::asio::awaitable<void> do_session(boost::asio::ip::tcp::socket& client_socket) {
        auto [client_response, client_headers_size] = co_await read_headers(client_socket);
        const std::string_view client_headers(client_response.data(), client_headers_size);
        const auto [host, port] = findHostPort(client_headers);

        auto external_socket = co_await connect_to_external_host(host, port);

        co_await boost::asio::async_write(external_socket, boost::asio::buffer(client_response),
                                          boost::asio::use_awaitable);

        auto [external_response, external_headers_size] = co_await read_headers(external_socket);
        const std::string_view external_headers(external_response.data(), external_headers_size);
        const auto external_content_size = findContentLength(external_headers);

        co_await boost::asio::async_write(client_socket, boost::asio::buffer(external_response),
                                          boost::asio::use_awaitable);

        if (!external_content_size.has_value()) {
            co_return;
        }

        const size_t already_copied_size = external_response.size() - external_headers_size;
        co_await copy_response(external_socket, client_socket, *external_content_size, already_copied_size);

        co_return;
    }

    boost::asio::awaitable<std::pair<std::string, size_t>> read_headers(boost::asio::ip::tcp::socket& socket) {
        std::string headers;
        const size_t headers_size = co_await boost::asio::async_read_until(
            socket, boost::asio::dynamic_buffer(headers), headers_delimiter, boost::asio::use_awaitable);

        co_return std::pair{std::move(headers), headers_size};
    }

    boost::asio::awaitable<boost::asio::ip::tcp::socket> connect_to_external_host(std::string_view host,
                                                                                  std::string_view port) {
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::ip::tcp::resolver resolver(executor);
        boost::asio::ip::tcp::socket external_socket(executor);

        const auto external_addrs = co_await resolver.async_resolve(host, port, boost::asio::use_awaitable);
        co_await boost::asio::async_connect(external_socket, external_addrs, boost::asio::use_awaitable);

        co_return std::move(external_socket);
    }

    boost::asio::awaitable<void> copy_response(boost::asio::ip::tcp::socket& src_socket,
                                               boost::asio::ip::tcp::socket& dst_socket, size_t total_size,
                                               size_t read_size) {
        while (read_size < total_size) {
            char buffer[4096];
            const size_t remaining_size = total_size - read_size;
            const size_t buffer_size = co_await boost::asio::async_read(
                src_socket, boost::asio::buffer(buffer, std::min(sizeof(buffer), remaining_size)),
                boost::asio::transfer_at_least(1), boost::asio::use_awaitable);

            co_await boost::asio::async_write(dst_socket, boost::asio::buffer(buffer, buffer_size),
                                              boost::asio::use_awaitable);

            read_size += buffer_size;
        }

        co_return;
    }
};

class Server {
public:
    Server(boost::asio::io_context& io_context, short port)
        : io_context_(io_context),
          acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                boost::asio::co_spawn(io_context_, Handler{}.session(std::move(socket)), boost::asio::detached);
            } else {
                std::println(std::cerr, "{}", std::format("Accept error: {}", ec.message()));
            }
            do_accept();
        });
    }

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::println(std::cerr, "Usage: {} <listen_port>", argv[0]);
            return 1;
        }
        boost::asio::io_context io_context(1);
        Server server(io_context, std::atoi(argv[1]));
        io_context.run();
    } catch (const std::exception& e) {
        std::println(std::cerr, "{}", std::format("Exception: {}", e.what()));
    }
}
