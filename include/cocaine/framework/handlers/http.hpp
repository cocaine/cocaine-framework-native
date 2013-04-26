#ifndef COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP
#define COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP

#include <cocaine/framework/application.hpp>

#include <string>
#include <vector>
#include <memory>

#include <boost/optional.hpp>

namespace cocaine { namespace framework {

class http_request {
public:
    explicit http_request(const char *chunk, size_t size) {
        msgpack::unpacked msg;
        msgpack::unpack(&msg, chunk, size);

        msgpack::object_kv* req_it = msg.get().via.map.ptr;
        msgpack::object_kv* const pend = req_it + msg.get().via.map.size;

        for (; req_it < pend; ++req_it) {
            std::string key;
            req_it->key.convert(&key);

            if(key.compare("body") == 0) {
                req_it->val.convert(&m_body);
            } else if (key.compare("request") == 0) {
                req_it->val.convert(&m_request);
            } else if (key.compare("meta") == 0) {
                msgpack::object_kv* meta_it = req_it->val.via.map.ptr;
                msgpack::object_kv* const pend = meta_it + req_it->val.via.map.size;

                for (; meta_it < pend; ++meta_it) {
                    std::string key;
                    meta_it->key.convert(&key);

                    if(key.compare("headers") == 0) {
                        meta_it->val.convert(&m_headers);
                    } else if (key.compare("cookies") == 0) {
                        meta_it->val.convert(&m_cookies);
                    } else if (meta_it->val.type == msgpack::type::RAW) {
                        m_meta[key] = meta_it->val.as<std::string>();
                    }
                }
            }
        }
    }

    const std::string&
    body() const {
        return m_body;
    }

    const std::map<std::string, std::string>&
    request() const {
        return m_request;
    }

    const std::string&
    request(const std::string& key) const {
        return m_request.at(key);
    }

    const std::map<std::string, std::string>&
    headers() const {
        return m_headers;
    }

    const std::string&
    header(const std::string& key) const {
        return m_headers.at(key);
    }

    const std::map<std::string, std::string>&
    cookies() const {
        return m_cookies;
    }

    const std::string&
    cookie(const std::string& key) const {
        return m_cookies.at(key);
    }

    const std::map<std::string, std::string>&
    meta() const {
        return m_meta;
    }

    const std::string&
    meta(const std::string& key) const {
        return m_meta.at(key);
    }

private:
    std::string m_body;
    std::map<std::string, std::string> m_request;
    std::map<std::string, std::string> m_meta;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_cookies;

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
    http_handler(std::shared_ptr<App> a) :
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
    error(const std::string& message) {
        if (!response()->closed()) {
            response()->error(cocaine::invocation_error, message);
        }
    }

    void
    send_response(const http_response& r) {
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
                response()->write(r.body()->data(), r.body()->size());
            }

            response()->close();
        }
    }

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

    template<class... Args>
    void
    ok(Args&&... args) {
        send_response(200, std::forward<Args>(args)...);
    }

    template<class... Args>
    void
    bad_request(Args&&... args) {
        send_response(400, std::forward<Args>(args)...);
    }

    template<class... Args>
    void
    not_found(Args&&... args) {
        send_response(404, std::forward<Args>(args)...);
    }

private:
    using cocaine::framework::handler<App>::response;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP
