#ifndef COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP
#define COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP

#include <cocaine/framework/handler.hpp>

#include <msgpack.hpp>

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include <boost/optional.hpp>

namespace cocaine { namespace framework {

class http_request {
public:
    explicit http_request(const char *chunk,
                          size_t size);

    const std::string&
    body() const {
        return m_body;
    }

    const std::map<std::string, std::vector<std::string>>&
    request() const {
        return m_request;
    }

    boost::optional<std::string>
    arg(const std::string& key) const {
        auto it = m_request.find(key);
        if (it != m_request.end()) {
            return boost::make_optional(it->second[0]);
        } else {
            return boost::optional<std::string>();
        }
    }

    void
    arg(const std::string& key,
        boost::optional<std::vector<std::string>>& result) const
    {
        auto it = m_request.find(key);
        if (it != m_request.end()) {
            result.reset(it->second);
        } else {
            result.reset();
        }
    }

    const std::map<std::string, std::string>&
    headers() const {
        return m_headers;
    }

    boost::optional<std::string>
    header(const std::string& key) const {
        auto it = m_headers.find(key);
        if (it != m_headers.end()) {
            return boost::make_optional(it->second);
        } else {
            return boost::optional<std::string>();
        }
    }

    const std::map<std::string, std::string>&
    cookies() const {
        return m_cookies;
    }

    boost::optional<std::string>
    cookie(const std::string& key) const {
        auto it = m_cookies.find(key);
        if (it != m_cookies.end()) {
            return boost::make_optional(it->second);
        } else {
            return boost::optional<std::string>();
        }
    }

    const std::map<std::string, std::string>&
    meta() const {
        return m_meta;
    }

    const std::string&
    meta(const std::string& key) const {
        return m_meta.at(key);
    }

    bool
    secure() const {
        return  m_secure;
    }

    const std::string&
    method() const {
        return meta("method");
    }

    const std::string&
    url() const {
        return meta("url");
    }

    const std::string&
    host() const {
        return meta("host");
    }

    const std::string&
    path() const {
        return meta("script_name");
    }

    const std::string&
    remote_address() const {
        return meta("remote_addr");
    }

    const std::string&
    server_address() const {
        return meta("server_addr");
    }

private:
    std::string m_body;
    std::map<std::string, std::vector<std::string>> m_request;
    std::map<std::string, std::string> m_meta;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_cookies;
    bool m_secure;
};

class http_response {
public:
    typedef std::vector<std::pair<std::string, std::string>> headers_t;

public:
    explicit http_response(int code = 200, // i'm optimistic
                           const headers_t& headers = headers_t()) :
        m_code(code),
        m_headers(headers)
    {
        // pass
    }

    http_response(int code,
                  const headers_t& headers,
                  const std::string& body) :
        m_code(code),
        m_headers(headers),
        m_body(body)
    {
        // pass
    }

    void
    add_header(const std::string& header,
               const std::string& value)
    {
        m_headers.push_back(std::make_pair(header, value));
    }

    void
    set_content_type(const std::string& type);

    void
    set_code(int code) {
        m_code = code;
    }

    void
    set_body(const std::string& body) {
        m_body = body;
    }

    int
    code() const {
        return m_code;
    }

    const headers_t&
    headers() const {
        return m_headers;
    }

    const boost::optional<std::string>&
    body() const {
        return m_body;
    }

private:
    int m_code;
    headers_t m_headers;
    boost::optional<std::string> m_body;
};

template<class App>
struct http_handler :
    public cocaine::framework::handler<App>
{
    http_handler(App *a) :
        cocaine::framework::handler<App>(a)
    {
        // pass
    }

    virtual
    void
    on_request(const http_request&) = 0;

    void
    on_chunk(const char *chunk, size_t size) {
        on_request(http_request(chunk, size));
    }

    void
    send_response(const http_response& r);

    void
    send_response(int code,
                  const std::vector<std::pair<std::string, std::string>>& headers,
                  const std::string& body)
    {
        send_response(http_response(code, headers, body));
    }

    void
    send_response(int code,
                  const http_response::headers_t& headers = http_response::headers_t())
    {
        send_response(http_response(code, headers));
    }

private:
    using cocaine::framework::handler<App>::response;
};

template<class App>
void
http_handler<App>::send_response(const http_response& r) {
    if (!response()->closed()) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack_map(2);
        pk.pack(std::string("code"));
        pk.pack(r.code());
        pk.pack(std::string("headers"));
        pk.pack(r.headers());
        response()->write(buffer.data(), buffer.size());

        if (r.body()) {
            response()->write(r.body().get());
        }

        response()->close();
    } else {
        throw std::logic_error("Http response has been already sent");
    }
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP
