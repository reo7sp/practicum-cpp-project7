#include "headers.h"

#include <charconv>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

void iterHeaders(std::string_view req, Callback&& callback) {
    auto lines = req | std::views::split(std::string_view{"\r\n"}) | std::views::drop(1);

    for (auto&& line_range : lines) {
        const std::string_view line(line_range.begin(), line_range.end());
        if (line.empty()) {
            break;
        }

        const size_t separator = line.find(':');
        if (separator == std::string_view::npos) {
            continue;
        }

        std::string_view name = line.substr(0, separator);
        std::string_view value = line.substr(separator + 1);
        if (!value.empty() && value.front() == ' ') {
            value.remove_prefix(1);
        }

        callback(name, value);
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::pair<std::string, std::string> host_port;

    iterHeaders(req, [&host_port](std::string_view name, std::string_view value) {
        if (name != "Host") {
            return;
        }

        const size_t separator = value.find(':');
        if (separator == std::string_view::npos) {
            host_port.first = std::string(value);
            return;
        }

        host_port.first = std::string(value.substr(0, separator));
        host_port.second = std::string(value.substr(separator + 1));
    });

    if (host_port.first.empty()) {
        throw std::runtime_error("Host header is required");
    }

    if (host_port.second.empty()) {
        host_port.second = "80";
    }

    return host_port;
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> content_length;

    iterHeaders(rsp, [&content_length](std::string_view name, std::string_view value) {
        if (name != "Content-Length" || content_length.has_value()) {
            return;
        }

        size_t parsed_value = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
        if (ec == std::errc() && ptr == value.data() + value.size()) {
            content_length = parsed_value;
        }
    });

    return content_length;
}
