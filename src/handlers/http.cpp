#include <cocaine/framework/handlers/http.hpp>

#include <boost/algorithm/string.hpp>

using namespace cocaine::framework;

http_request::http_request(const char *chunk,
                           size_t size)
{
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
            msgpack::object_kv* it = req_it->val.via.map.ptr;
            msgpack::object_kv* const pend = it + req_it->val.via.map.size;

            for (; it < pend; ++it) {
                if (it->val.type == msgpack::type::RAW) {
                    m_request[it->key.as<std::string>()].push_back(it->val.as<std::string>());
                } else if (it->val.type == msgpack::type::ARRAY) {
                    m_request[it->key.as<std::string>()] = it->val.as<std::vector<std::string>>();
                }
            }
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
                } else if (key.compare("secure") == 0) {
                    meta_it->val.convert(&m_secure);
                } else if (meta_it->val.type == msgpack::type::RAW) {
                    m_meta[key] = meta_it->val.as<std::string>();
                }
            }
        }
    }
}

void
http_response::set_content_type(const std::string& type) {
    for (auto header = m_headers.begin(); header != m_headers.end(); ++header) {
        if (boost::iequals(header->first, "Content-Type")) {
            header->second = type;
            return;
        }
    }
    // else
    add_header("Content-Type", type);
}

