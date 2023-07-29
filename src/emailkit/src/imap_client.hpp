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

class imap_client_t {
   public:
    virtual ~imap_client_t() = default;
    virtual void start() = 0;
    virtual void on_state_change(std::function<void(imap_client_state)>) = 0;

    virtual void async_connect(std::string host, std::string port, async_callback<void> cb) = 0;
    virtual void async_obtain_capabilities(async_callback<std::vector<std::string>> cb) = 0;
};

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx);

}  // namespace emailkit