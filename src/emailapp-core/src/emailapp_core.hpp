#include <emailkit/global.hpp>
#include <asio/io_context.hpp>

namespace emailapp::core {
class EmailAppCore {
   public:
    virtual ~EmailAppCore() = default;
    virtual void async_run(async_callback<void> cb) = 0;
};

std::shared_ptr<EmailAppCore> make_emailapp_core(asio::io_context& ctx);

}  // namespace emailapp::core