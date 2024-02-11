#include <emailkit/global.hpp>
#include <asio/io_context.hpp>

namespace emailapp::core {
struct Account {};

class EmailAppCoreCallbacks {
   public:
    virtual ~EmailAppCoreCallbacks() = default;

    // Is called by EmailAppCore when it needs to get credentails to work with.
    virtual void async_provide_credentials(async_callback<std::vector<Account>> cb) = 0;
};

class EmailAppCore {
   public:
    virtual ~EmailAppCore() = default;
    virtual void async_run(async_callback<void> cb) = 0;
    virtual void add_acount(Account acc) = 0;
};

std::shared_ptr<EmailAppCore> make_emailapp_core(asio::io_context& ctx, EmailAppCoreCallbacks& cbs);

}  // namespace emailapp::core
