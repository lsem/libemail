#pragma once
#include <emailkit/global.hpp>
#include <asio/io_context.hpp>

namespace mailer {

class MailerPOC {
   public:
    virtual ~MailerPOC() = default;
    virtual void async_run(async_callback<void> cb) = 0;
};

std::shared_ptr<MailerPOC> make_mailer_poc(asio::io_context& ctx);
}  // namespace mailer
