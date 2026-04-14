#include "headers.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

TEST(iterHeaders, EmptyRequest) {
    size_t calls_count = 0;
    iterHeaders("", [&calls_count](std::string_view, std::string_view) { ++calls_count; });

    EXPECT_EQ(calls_count, 0);
}

TEST(iterHeaders, Empty) {
    size_t calls_count = 0;
    iterHeaders("GET / HTTP/1.1\r\n\r\n", [&calls_count](std::string_view, std::string_view) { ++calls_count; });

    EXPECT_EQ(calls_count, 0);
}

TEST(iterHeaders, SkipRequestLine) {
    std::string header_name;
    std::string header_value;

    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
                [&header_name, &header_value](std::string_view name, std::string_view value) {
                    header_name = std::string(name);
                    header_value = std::string(value);
                });

    EXPECT_EQ(header_name, "Host");
    EXPECT_EQ(header_value, "example.com");
}

TEST(iterHeaders, SingleHeader) {
    std::vector<std::pair<std::string, std::string>> headers;

    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
                [&headers](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });

    ASSERT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) {
    std::vector<std::pair<std::string, std::string>> headers;

    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\n",
                [&headers](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });

    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0], std::make_pair(std::string("Host"), std::string("example.com")));
    EXPECT_EQ(headers[1], std::make_pair(std::string("Content-Length"), std::string("5")));
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::vector<std::pair<std::string, std::string>> headers;

    iterHeaders("GET / HTTP/1.1\r\nSet-Cookie: a=1\r\nSet-Cookie: b=2\r\n\r\n",
                [&headers](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });

    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0], std::make_pair(std::string("Set-Cookie"), std::string("a=1")));
    EXPECT_EQ(headers[1], std::make_pair(std::string("Set-Cookie"), std::string("b=2")));
}

TEST(findHostPort, Simple) {
    const auto [host, port] = findHostPort("GET / HTTP/1.1\r\nHost: example.com:8080\r\n\r\n");

    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "8080");
}

TEST(findHostPort, NoHost) {
    EXPECT_THROW(findHostPort("GET / HTTP/1.1\r\nUser-Agent: wget\r\n\r\n"), std::runtime_error);
}

TEST(findHostPort, DefaultPort) {
    const auto [host, port] = findHostPort("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");

    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "80");
}

TEST(findContentLength, Simple) {
    const auto content_length = findContentLength("HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n");

    ASSERT_TRUE(content_length.has_value());
    EXPECT_EQ(*content_length, 4096);
}

TEST(findContentLength, NoContentLength) {
    const auto content_length = findContentLength("HTTP/1.1 200 OK\r\nServer: test\r\n\r\n");

    EXPECT_FALSE(content_length.has_value());
}

TEST(findContentLength, EmptyBody) {
    const auto content_length = findContentLength("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    ASSERT_TRUE(content_length.has_value());
    EXPECT_EQ(*content_length, 0);
}

TEST(findContentLength, InvalidContentLength) {
    const auto content_length = findContentLength("HTTP/1.1 200 OK\r\nContent-Length: 12abc\r\n\r\n");

    EXPECT_FALSE(content_length.has_value());
}
