#include "imap_client.hpp"
#include <b64/naive.h>
#include <map>
#include "imap_socket.hpp"

namespace emailkit {

namespace {
class imap_client_impl_t : public imap_client_t {
   public:
    explicit imap_client_impl_t(asio::io_context& ctx) : m_ctx(ctx) {}

    bool initialize() {
        m_imap_socket = make_imap_socket(m_ctx);
        if (!m_imap_socket) {
            log_info("make_imap_socket failed");
            return false;
        }
        return true;
    }

    virtual void start() override {
        // what does start mean?
        //  I think it should set proper state and then work according to given state and desired
        //  state.
        // so when started, client wants to be connected to the server.
        // but only if it has new data.

        // ideally client should not be that smart and we should have some upper level logic
        // implementing smart things.
    }

    virtual void on_state_change(std::function<void(imap_client_state)> cb) override {
        m_state_change_cb = std::move(cb);
    }

    virtual void async_connect(std::string host,
                               std::string port,
                               async_callback<void> cb) override {
        assert(m_imap_socket);
        m_imap_socket->async_connect(host, port,
                                     [this, cb = std::move(cb)](std::error_code ec) mutable {
                                         if (!ec) {
                                             // just after connect we should start receiving data
                                             // from the server, just in case it decides to
                                             // disconnect us we should at least be able to write
                                             // this fact into the log.
                                             //  alterntive is to start on special command like
                                             //  ready_to_receive()
                                             recive_next_line();
                                         }
                                         cb(ec);
                                     });
    }

    void recive_next_line() {
        m_imap_socket->async_receive_line([this](std::error_code ec, std::string line) {
            if (ec) {
                if (ec == asio::error::eof) {
                    // socket closed
                    // TODO:
                    log_warning("socket closed");
                } else {
                    log_error("error reading line: {}", ec);
                }
                return;
            }
            log_debug("received line: {}", line);
            recive_next_line();
        });
    }

    virtual void async_obtain_capabilities(async_callback<std::vector<std::string>> cb) override {
        // IDEA: before sending command we register handler, or we can attach handler to
        // send_command itself. through some helper. execute_command(new_command_id(), "CAPABILITY",
        // [](std::string ) { response; }) we can add additional timeout (e.g. 5s). If command comes
        // without ID then we have unregistered response/upstream command.
        //
        const auto id = new_command_id();
        m_active_commands[id] = {.cb = [id](std::error_code ec) {
            log_debug("command {} finished: {}", id, ec.message());
            // ..
        }};

        m_imap_socket->async_send_command(fmt::format("{} CAPABILITY\r\n", id),
                                          [](std::error_code ec) {
                                              // ..
                                          });
    }

    virtual void async_authenticate(xoauth2_creds_t creds, async_callback<void> cb) override {
        // prepare XOAUTH2 request
        // https://developers.google.com/gmail/imap/xoauth2-protocol
        // https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth
        const std::string xoauth2_req =
            fmt::format("user={}^Auth=Bearer {}^A^A", creds.user_email, creds.oauth_token);
        log_debug("xoauth2_req: {}", xoauth2_req);
        const std::string xoauth2_req_encoded = b64::base64_naive_encode(xoauth2_req);

        // TODO: don't use this.
        const auto id = new_command_id();

        m_active_commands[id] = {.cb = [id, cb = std::move(cb)](std::error_code ec) mutable {
            log_debug("command {} finished: {}", id, ec);
            cb(ec);
        }};

        m_imap_socket->async_send_command(
            fmt::format("{} AUTHENTICATE XOAUTH2 {}\r\n", id, xoauth2_req_encoded),
            [this](std::error_code ec) {
                if (ec) {
                    // TODO: remove active command.
                    log_error("'AUTHENTICATE XOAUTH2' command failed: {}", ec);
                    return;
                }
                log_debug("auth command send, waiting response");
            });
    }

    std::string new_command_id() { return std::string("A") + std::to_string(m_command_counter++); }

   private:
    asio::io_context& m_ctx;
    std::shared_ptr<imap_socket_t> m_imap_socket;
    std::function<void(imap_client_state)> m_state_change_cb = [](auto s) {};
    int m_command_counter = 0;

    // a registry of pending commands for which we are waiting response.
    struct pending_command_ctx {
        async_callback<void> cb;
    };
    std::map<std::string, pending_command_ctx> m_active_commands;
};

}  // namespace

std::shared_ptr<imap_client_t> make_imap_client(asio::io_context& ctx) {
    auto client = std::make_shared<imap_client_impl_t>(ctx);
    if (!client->initialize()) {
        return nullptr;
    }
    return client;
}

}  // namespace emailkit