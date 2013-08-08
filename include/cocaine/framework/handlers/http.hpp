#ifndef COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP
#define COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP

#include <cocaine/framework/handler.hpp>
#include <cocaine/framework/common.hpp>

#include <cocaine/traits.hpp>
#include <cocaine/traits/typelist.hpp>

#include <string>
#include <vector>
#include <memory>

namespace cocaine { namespace framework {

class http_headers_t {
    typedef std::vector<std::pair<std::string, std::string>>
            headers_vector_t;

public:
    explicit
    http_headers_t(const headers_vector_t& headers = headers_vector_t()) :
        m_headers(headers)
    {
        // pass
    }

    http_headers_t(headers_vector_t&& headers) :
        m_headers(std::move(headers))
    {
        // pass
    }

    http_headers_t(const http_headers_t& other) :
        m_headers(other.m_headers)
    {
        // pass
    }

    http_headers_t(http_headers_t&& other) :
        m_headers(std::move(other.m_headers))
    {
        // pass
    }

    const headers_vector_t&
    data() const {
        return m_headers;
    }

    std::vector<std::string>
    headers(const std::string& key) const;

    boost::optional<std::string>
    header(const std::string& key) const;

    // add new entry
    void
    add_header(const std::string& key,
               const std::string& value);

    // remove all previous 'key' entries and add new ones
    void
    reset_header(const std::string& key,
                 const std::vector<std::string>& values);

    // same as reset_header(key, std::vector<std::string> {value})
    void
    reset_header(const std::string& key,
                 const std::string& value);

private:
    headers_vector_t m_headers;
};

struct http_request_t {
    friend struct cocaine::io::type_traits<http_request_t>;

    http_request_t() {
        // pass
    }

    http_request_t(const std::string& method,
                   const std::string& uri,
                   const std::string& http_version,
                   const http_headers_t& headers,
                   const std::string& body) :
        m_method(method),
        m_uri(uri),
        m_version(http_version),
        m_headers(headers),
        m_body(body)
    {
        // pass
    }

    const std::string&
    method() const {
        return m_method;
    }

    void
    set_method(const std::string& method) {
        m_method = method;
    }

    const std::string&
    uri() const {
        return m_uri;
    }

    void
    set_uri(const std::string& uri) {
        m_uri = uri;
    }

    const std::string&
    http_version() const {
        return m_version;
    }

    void
    set_http_version(const std::string& version) {
        m_version = version;
    }

    const http_headers_t&
    headers() const {
        return m_headers;
    }

    void
    set_headers(const http_headers_t& headers) {
        m_headers = headers;
    }

    const std::string&
    body() const {
        return m_body;
    }

    void
    set_body(const std::string& body) {
        m_body = body;
    }

private:
    std::string m_method;
    std::string m_uri;
    std::string m_version;
    http_headers_t m_headers;
    std::string m_body;
};

}} // namespace cocaine::framework

namespace cocaine { namespace io {

template<>
struct type_traits<cocaine::framework::http_headers_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const cocaine::framework::http_headers_t& source) {
        packer << source.data();
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, cocaine::framework::http_headers_t& target) {
        std::vector<std::pair<std::string, std::string>> headers;
        unpacked >> headers;
        target = cocaine::framework::http_headers_t(std::move(headers));
    }
};

template<>
struct type_traits<cocaine::framework::http_request_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const cocaine::framework::http_request_t& source) {
        type_traits<boost::mpl::list<std::string,
                                     std::string,
                                     std::string,
                                     cocaine::framework::http_headers_t,
                                     std::string>>
        ::pack(packer, source.m_method, source.m_uri, source.m_version, source.m_headers, source.m_body);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, cocaine::framework::http_request_t& target) {
        type_traits<boost::mpl::list<std::string,
                                     std::string,
                                     std::string,
                                     cocaine::framework::http_headers_t,
                                     std::string>>
        ::unpack(unpacked, target.m_method, target.m_uri, target.m_version, target.m_headers, target.m_body);
    }
};

}} // namespace cocaine::io

namespace cocaine { namespace framework {

struct http_upstream_t
{
    http_upstream_t(std::shared_ptr<upstream_t> s) :
        m_stream(s)
    {
        // pass
    }

    ~http_upstream_t() {
        if (!closed()) {
            close();
        }
    }

    void
    write_headers(int code,
                  const http_headers_t& headers)
    {
        m_stream->write(std::make_tuple(code, headers));
    }

    void
    write(const std::string& body)
    {
        m_stream->write(body);
    }

    void
    close() {
        m_stream->close();
    }

    bool
    closed() const {
        return m_stream->closed();
    }

private:
    std::shared_ptr<upstream_t> m_stream;
};

template<class App>
struct http_handler :
    public handler<App>
{
    http_handler(App &a) :
        handler<App>(a)
    {
        // pass
    }

    virtual
    void
    on_request(const http_request_t&) = 0;

    void
    on_chunk(const char *chunk,
             size_t size)
    {
        m_upstream = std::make_shared<http_upstream_t>(handler<App>::response());
        on_request(unpack<http_request_t>(chunk, size));
    }

    std::shared_ptr<http_upstream_t>
    response() {
        return m_upstream;
    }

private:
    using cocaine::framework::handler<App>::response;

private:
    std::shared_ptr<http_upstream_t> m_upstream;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_HTTP_HPP
