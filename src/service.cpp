#include <cocaine/framework/service.hpp>

/*
 * error_code
 */

namespace {

    struct service_client_category_impl :
        public std::error_category
    {
        const char*
        name() const throw() {
            return "service client";
        }

        std::string
        message(int error_value) const throw() {
            switch (error_value) {
                case static_cast<int>(cocaine::framework::service_errc::bad_version):
                    return "Bad version of service";

                case static_cast<int>(cocaine::framework::service_errc::not_connected):
                    return "There is no connection to service";

                case static_cast<int>(cocaine::framework::service_errc::wait_for_connection):
                    return "Client is reconnecting to service at now";

                case static_cast<int>(cocaine::framework::service_errc::broken_manager):
                    return "Service manager no longer exists";

                default:
                    return "Unknown service error o_O";
            }
        }
    };

    struct service_response_category_impl :
        public std::error_category
    {
        const char*
        name() const throw() {
            return "service response";
        }

        std::string
        message(int /* error_value */) const throw() {
            return "Error from service";
        }
    };

    service_client_category_impl client_category;
    service_response_category_impl response_category;

} // namespace

const std::error_category&
cocaine::framework::service_client_category() {
    return client_category;
}

const std::error_category&
cocaine::framework::service_response_category() {
    return response_category;
}

std::error_code
cocaine::framework::make_error_code(cocaine::framework::service_errc e) {
    return std::error_code(static_cast<int>(e), cocaine::framework::service_client_category());
}

std::error_condition
cocaine::framework::make_error_condition(cocaine::framework::service_errc e) {
    return std::error_condition(static_cast<int>(e), cocaine::framework::service_client_category());
}

/*
 * service_t
 */

using namespace cocaine::framework;

service_t::service_t(std::shared_ptr<service_connection_t> connection) :
    m_connection(connection)
{
    auto m = connection->m_manager.lock();
    if (m) {
        m->register_connection(connection);
    }
}

service_t::~service_t() {
    auto c = m_connection.lock();
    if (c) {
        auto m = c->m_manager.lock();
        if (m) {
            m->release_connection(c);
        }
    }
}

std::string
service_t::name() {
    return connection()->name();
}

service_status
service_t::status() {
    return connection()->status();
}

void
service_t::reconnect() {
    connection()->reconnect().get();
}

namespace {
    void
    empty(future<std::shared_ptr<service_connection_t>>& f) {
        f.get();
    }
}

future<void>
service_t::async_reconnect() {
    return connection()->reconnect().then(&empty);
}

void
service_t::soft_destroy() {
    connection()->soft_destroy();
    m_connection.reset();
}

std::shared_ptr<service_connection_t>
service_t::connection() {
    auto c = m_connection.lock();
    if (c) {
        return c;
    } else {
        throw service_error_t(service_errc::broken_manager);
    }
}
