#include <asio/io_context.hpp>
#include <emailkit/global.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace emailkit {

enum class imap_client_state {
    not_connected,
    connected,
    authorized,
};

struct xoauth2_creds_t {
    std::string user_email;
    std::string oauth_token;
};

struct oauthbearer_creds_t {
    std::string user_email;
    std::string host;
    std::string port;
    std::string oauth_token;
};

struct auth_error_details_t {
    std::string summary;
};

class imap_client_t {
   public:
    virtual ~imap_client_t() = default;
    virtual void start() = 0;
    virtual void on_state_change(std::function<void(imap_client_state)>) = 0;

    virtual void async_connect(std::string host, std::string port, async_callback<void> cb) = 0;
    virtual void async_obtain_capabilities(async_callback<std::vector<std::string>> cb) = 0;
    virtual void async_authenticate(xoauth2_creds_t creds,
                                    async_callback<auth_error_details_t> cb) = 0;

    // TODO: https://www.rfc-editor.org/rfc/rfc7628.html
    virtual void async_authenticate(oauthbearer_creds_t creds,
                                    async_callback<auth_error_details_t> cb) {}

    // TODO: state change API (logical states + disconnected/failed)
};

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx);

}  // namespace emailkit