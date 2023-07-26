#include "emailkit/emailkit.hpp"

namespace emailkit {

imap_client_t::imap_client_t(asio::io_context &ctx): m_ctx(ctx) {
}

void imap_client_t::async_connect(std::string uri, async_callback<void> cb) 
{
    
}


}