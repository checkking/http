#include <gmock/gmock.h>

#include "Responder.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <time.h> // some functions aren't in <ctime>

using namespace ::testing;

namespace Yam {
namespace Http {

namespace {

class StringOutputStream : public OutputStream {
public:
    std::size_t Write(const void* buffer, std::size_t bufferSize) override {
        _stream.write(static_cast<const char*>(buffer), bufferSize);
        return bufferSize;
    }

    std::string ToString() const {
        return _stream.str();
    }

private:
    std::ostringstream _stream;
};

} // unnamed namespace

class ResponderTest : public Test {
protected:
    auto MakeResponder(std::shared_ptr<StringOutputStream> s) const {
        return std::make_unique<Responder>(std::move(s));
    }

    auto MakeStream() const {
        return std::make_shared<StringOutputStream>();
    }

    auto MakeBodyFromString(const std::string& s) const {
        return std::make_shared<std::vector<char>>(s.data(), s.data() + s.size());
    }
};

TEST_F(ResponderTest, just_status) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->Send(Status::Continue);

    EXPECT_EQ("HTTP/1.1 100 Continue\r\n\r\n", stream->ToString());
}

TEST_F(ResponderTest, some_headers) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->SetField("First", "Hello world!");
    r->SetField("Second", "v4r!0u$ sYm80;5");
    r->Send(Status::Ok);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "First: Hello world!\r\n"
        "Second: v4r!0u$ sYm80;5\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, headers_and_body) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->SetBody(MakeBodyFromString("Hello world!"));
    r->Send(Status::BadGateway);

    auto expected = "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello world!";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, simple_cookies) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->SetCookie("First", "One");
    r->SetCookie("Second", "Two");
    r->Send(Status::NotFound);

    auto expected = "HTTP/1.1 404 Not Found\r\n"
        "Set-Cookie: First=One\r\n"
        "Set-Cookie: Second=Two\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, cookie_with_simple_options) {
    using namespace std::literals;

    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    CookieOptions opts;
    opts.SetDomain("example.com");
    opts.SetPath("/some/path");
    opts.SetMaxAge(10min);

    r->SetCookie("First", "One", opts);
    r->Send(Status::Ok);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; Domain=example.com; Path=/some/path; Max-Age=600\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, cookie_with_expiration_date) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    struct tm tm;
    if (!::strptime("2013-01-15 21:47:38", "%Y-%m-%d %T", &tm))
        throw std::logic_error("Bad call to strptime()");
    auto t = ::timegm(&tm);

    CookieOptions opts;
    opts.SetExpiration(t);

    r->SetCookie("First", "One", opts);
    r->Send(Status::Ok);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; Expires=Tue, 15 Jan 2013 21:47:38 GMT\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, cookie_with_httponly_and_secure) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    CookieOptions opts;
    opts.SetHttpOnly();
    opts.SetSecure();

    r->SetCookie("First", "One", opts);
    r->Send(Status::Ok);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; HttpOnly; Secure\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

} // namespace Http
} // namespace Yam

