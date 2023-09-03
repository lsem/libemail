#include "imap_socket.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <fstream>
#include <regex>
#include <system_error>

#include "utils.hpp"

#define RAISE_CB_ON_ERROR(ec)                     \
    do {                                          \
        if (ec) {                                 \
            log_error("error: {}", ec.message()); \
            cb(ec);                               \
            return;                               \
        }                                         \
    } while (false)

template <class... T>
struct spell_type;

namespace emailkit {
using iterator_t = asio::buffers_iterator<asio::const_buffers_1>;
struct imap_match_condition_t {
    explicit imap_match_condition_t(std::string tag)
        : m_tag(tag), m_pattern_prefix{fmt::format("{} ", m_tag)} {}

    enum class state_t { idle, reading_size, waiting_cr, waiting_lf, reading_data, got_tag };

    std::pair<iterator_t, bool> operator()(iterator_t begin, iterator_t end) {
        if (std::exchange(m_first_byte, false)) {
            log_debug("started reading response");
        }

        // FIXME:
        // we are looking for a pattern according to grammar. hoping there cannot be sequence like
        // that (but we need to check to create nstring/string somewhere which looks like this). in
        // general this is going to be flawed until we have real streaming parsing when we read and
        // parse on the fly.

        iterator_t it = begin;
        for (; it != end; ++it) {
            if (m_state == state_t::got_tag) {
                log_debug("in state got tag, char is: {} ({})", (int)*it,
                          isprint(*it) ? ((char)*it) : '?');
                // TODO: should we somehow recover from this?
                // we can validate this tag better, strictly accordint the grammar!
                if (m_prev == 0x0d && *it == 0x0a) {
                    log_debug("finished reading response");
                    return {it + 1, true};
                }
                // m_prev = *it;
            } else if (m_state == state_t::idle) {
                // this is main part of the matcher:
                //  in the stream we try to find either literal size '{'
                // or \r\nTAG or if it is beginning of the line, "TAG "
                if (*it == '{') {
                    m_state = state_t::reading_size;
                    m_literal_size_s = "";
                } else if (m_prev == 0x0d && *it == 0x0a) {
                    m_line_size = 0;
                } else {
                    // TODO: it seems like we can make things safer by expecting that TAG is either
                    // goes at tge beginning ro it is after \r\n but not somewhere in the line.
                    if (*it == m_pattern_prefix[m_next_expected_prefix_char_idx] &&
                        m_next_expected_prefix_char_idx == m_line_size) {
                        log_debug("matched {}", (int)*it);
                        m_next_expected_prefix_char_idx++;
                        if (m_next_expected_prefix_char_idx == m_pattern_prefix.size()) {
                            // TODO: double check that after that we have OK|NO\BAD. this can also
                            // be done in NFA fashion.
                            log_debug("matched all characters of prefix");
                            m_state = state_t::got_tag;
                            // return {it, true};
                        }
                    }
                    m_line_size++;
                    // maintain a window of last N characters and compare it with a pattern
                    // "\r\n{TAG} (OK|NO|BAD).*)" once we found it,
                }
            } else if (m_state == state_t::reading_size) {
                if (*it == '}') {
                    m_state = state_t::waiting_cr;
                } else {
                    if (isdigit(*it)) {
                        m_literal_size_s += *it;
                    } else {
                        // TODO: error!
                    }
                }
            } else if (m_state == state_t::waiting_cr) {
                if (*it == 0x0d) {
                    m_state = state_t::waiting_lf;
                } else {
                    // error, go to idle.
                    m_state = state_t::idle;
                    m_next_expected_prefix_char_idx = 0;
                }
            } else if (m_state == state_t::waiting_lf) {
                if (*it == 0x0a) {
                    m_state = state_t::reading_data;
                    m_literal_data_bytes_left = std::stoi(m_literal_size_s);
                    log_debug("reading literal data of size (size={})", m_literal_data_bytes_left);
                    if (m_literal_data_bytes_left > 0) {
                    } else {
                        log_debug("literal size == 0 case, going to idle state");
                        m_state = state_t::idle;
                        m_next_expected_prefix_char_idx = 0;
                    }
                } else {
                    // error, go to idle.
                    m_state = state_t::idle;
                    m_next_expected_prefix_char_idx = 0;
                }
            } else if (m_state == state_t::reading_data) {
                if (m_literal_data_bytes_left-- == 0) {
                    log_debug("read last byte of literal, going to idle state");
                    m_state = state_t::idle;
                    m_next_expected_prefix_char_idx = 0;
                }
            }

            m_prev = *it;
        }

        log_debug("added for match -- END");
        return {end, false};
    }

    std::string m_tag;
    size_t m_literal_data_bytes_left = 0;
    state_t m_state = state_t::idle;
    std::string m_literal_size_s;
    size_t m_next_expected_prefix_char_idx = 0;
    const std::string m_pattern_prefix;
    unsigned char m_prev = 255;
    bool m_first_byte = true;
    bool m_new_line = true;  // is true whenever we are in indle state and iterator it points to the
                             // first character of the line.
    size_t m_line_size = 0;
};  // namespace emailkit
}  // namespace emailkit

namespace asio {
template <>
struct is_match_condition<emailkit::imap_match_condition_t> : public std::true_type {};
}  // namespace asio

namespace emailkit {

namespace {

class match_char {
   public:
    // explicit match_char(char c) : c_(c) {}

    template <typename Iterator>
    std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const {
        // Iterator i = begin;
        // while (i != end)
        //   if (c_ == *i++)
        //     return std::make_pair(i, true);
        return std::make_pair(end, false);
    }

   private:
    char c_;
};

class imap_client_impl_t : public imap_socket_t, std::enable_shared_from_this<imap_client_impl_t> {
   public:
    explicit imap_client_impl_t(asio::io_context& ctx)
        : m_ctx(ctx), m_ssl_ctx(asio::ssl::context::sslv23), m_socket(m_ctx, m_ssl_ctx) {}

    virtual void async_connect(std::string host,
                               std::string port,
                               async_callback<void> cb) override {
        log_debug("async_connect is working ..");

        // TODO: async?
        asio::ip::tcp::resolver resolver{m_ctx};
        asio::ip::tcp::resolver::query query{host, port};

        std::error_code ec;
        auto endpoints = resolver.resolve(query, ec);
        if (ec) {
            log_error("resolve failed: {}", ec.message());
            cb(ec);
            return;
        }

        log_debug("addresses:");
        for (auto x : endpoints) {
            log_debug("ip address: {}", x.endpoint().address().to_string());
        }

        asio::async_connect(m_socket.lowest_layer(), std::move(endpoints),
                            [cb = std::move(cb), this](std::error_code ec,
                                                       const asio::ip::tcp::endpoint& e) mutable {
                                if (ec) {
                                    log_error("async_connect failed: {}", ec.message());
                                    cb(ec);
                                    return;
                                }

                                log_debug("connected: {}:{} (proto: {}), doing handhshake",
                                          e.address().to_string(), e.port(), e.protocol().family());
                                m_socket.async_handshake(
                                    asio::ssl::stream_base::client,
                                    [this, cb = std::move(cb)](std::error_code ec) mutable {
                                        if (ec) {
                                            log_error("async_handhshake failed: {}", ec.message());
                                            cb(ec);
                                            return;
                                        }
                                        log_debug("handshake done");

                                        m_connected = true;
                                        cb({});
                                    });
                            });
    }

    virtual void async_receive_line(async_callback<imap_response_line_t> cb) override {
        asio::async_read_until(
            m_socket, m_recv_buff, "\r\n",
            [this, cb = std::move(cb)](std::error_code ec, size_t bytes_transferred) mutable {
                if (ec) {
                    log_error("async_read_until failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                log_debug("bytes_transferred: {}, stream size: {}", bytes_transferred,
                          m_recv_buff.size());

                // TODO: ensure that stripping the line of \r\n is good idea. ABNF parses may not
                // like it and we may want to do it on upper level or have as parameter!

                const auto& buff_data = m_recv_buff.data();

                if (buff_data.size() < bytes_transferred) {
                    log_error("buff_data.size={} while bytes_transferred={}", buff_data.size(),
                              bytes_transferred);
                    cb(ec, {});
                    return;
                }

                const char* data_ptr = static_cast<const char*>(buff_data.data());

#ifndef NDEBUG
                if (m_opt_dump_stream_to_file) {
                    m_protocol_log_fs << "S: ";
                    m_protocol_log_fs.write(data_ptr, bytes_transferred);
                    if (!m_protocol_log_fs.good()) {
                        auto err = errno;
                        log_warning("write to protocol log failed: {}", strerror(err));
                    }
                    m_protocol_log_fs.flush();
                }
#endif

                std::string received_data{data_ptr, bytes_transferred};
                if (received_data.size() < 2 || received_data[received_data.size() - 2] != '\r' ||
                    received_data[received_data.size() - 1] != '\n') {
                    log_error("received not a line, no \\r\\n");
                    cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                m_recv_buff.consume(bytes_transferred);
                cb({}, imap_response_line_t{std::move(received_data)});
            });
    }

    virtual void async_receive_raw_line(async_callback<std::string> cb) override {
        asio::async_read_until(
            m_socket, m_recv_buff, "\r\n",
            [this, cb = std::move(cb)](std::error_code ec, size_t bytes_transferred) mutable {
                if (ec) {
                    log_error("async_read_until failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                log_debug("bytes_transferred: {}, stream size: {}", bytes_transferred,
                          m_recv_buff.size());

                // TODO: ensure that stripping the line of \r\n is good idea. ABNF parses may not
                // like it and we may want to do it on upper level or have as parameter!

                const auto& buff_data = m_recv_buff.data();

                if (buff_data.size() < bytes_transferred) {
                    log_error("buff_data.size={} while bytes_transferred={}", buff_data.size(),
                              bytes_transferred);
                    cb(ec, {});
                    return;
                }

                const char* data_ptr = static_cast<const char*>(buff_data.data());

#ifndef NDEBUG
                if (m_opt_dump_stream_to_file) {
                    // TODO: seems like we should not expect that write will happen by entire
                    // portion.
                    m_protocol_log_fs << "S: ";
                    m_protocol_log_fs.write(data_ptr, bytes_transferred);
                    if (!m_protocol_log_fs.good()) {
                        auto err = errno;
                        log_warning("write to protocol log failed: {}", strerror(err));
                    }

                    m_protocol_log_fs.flush();
                }
#endif

                std::string received_data{data_ptr, bytes_transferred};
                if (received_data.size() < 2 || received_data[received_data.size() - 2] != '\r' ||
                    received_data[received_data.size() - 1] != '\n') {
                    log_error("received not a line, no \\r\\n");
                    cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                m_recv_buff.consume(bytes_transferred);
                cb({}, std::move(received_data));
            });
    }

    virtual void async_receive_response(std::string tag, async_callback<std::string> cb) override {
        if (!m_connected) {
            log_error("not connected");
            cb(make_error_code(std::errc::not_connected), "");
            return;
        }

        imap_match_condition_t cond{tag};

        auto read_start_ts = std::chrono::steady_clock::now();

        asio::async_read_until(
            m_socket, m_recv_buff, cond,
            [read_start_ts, this, cb = std::move(cb)](std::error_code ec,
                                                      size_t bytes_transferred) mutable {
                if (ec) {
                    log_error("async_read_until failed: {}", ec);
                    cb(ec, {});
                    return;
                }

                auto time_taken = std::chrono::steady_clock::now() - read_start_ts;

                log_debug("bytes_transferred: {}, stream size: {}", bytes_transferred,
                          m_recv_buff.size());

                m_bytes_received_total += bytes_transferred;

                // TODO: ensure that stripping the line of \r\n is good idea. ABNF parses may not
                // like it and we may want to do it on upper level or have as parameter!

                const auto& buff_data = m_recv_buff.data();

                if (buff_data.size() < bytes_transferred) {
                    log_error("buff_data.size={} while bytes_transferred={}", buff_data.size(),
                              bytes_transferred);
                    cb(ec, {});
                    return;
                }

                const char* data_ptr = static_cast<const char*>(buff_data.data());

#ifndef NDEBUG
                if (m_opt_dump_stream_to_file) {
                    // TODO: seems like we should not expect that write will happen by entire
                    // portion.
                    m_protocol_log_fs << "S: ";
                    m_protocol_log_fs.write(data_ptr, bytes_transferred);
                    if (!m_protocol_log_fs.good()) {
                        auto err = errno;
                        log_warning("write to protocol log failed: {}", strerror(err));
                    }

                    m_protocol_log_fs.flush();
                }
#endif

                std::string received_data{data_ptr, bytes_transferred};
                if (received_data.size() < 2 || received_data[received_data.size() - 2] != '\r' ||
                    received_data[received_data.size() - 1] != '\n') {
                    log_error("received not a line, no \\r\\n");
                    cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                m_recv_buff.consume(bytes_transferred);
                cb({}, std::move(received_data));
            });
    }

    virtual void async_send_command(std::string command, async_callback<void> cb) override {
        if (!m_connected) {
            log_error("not connected");
            cb(make_error_code(std::errc::not_connected));
            return;
        }

        log_debug("sending command '{}'", utils::escape_ctrl(command));

        asio::async_write(
            m_socket, asio::buffer(command),
            [this, cb = std::move(cb), command](std::error_code ec, size_t bytes_written) mutable {
                if (ec) {
                    log_error("async_write failed: {}", ec.message());
                    cb(make_error_code(std::errc::not_connected));
                    return;
                }

#ifndef NDEBUG
                if (m_opt_dump_stream_to_file) {
                    // TODO: seems like we should not expect that write will happen by
                    // entire portion.
                    m_protocol_log_fs << "C: " << command;
                    if (!m_protocol_log_fs.good()) {
                        auto err = errno;
                        log_warning("write to protocol log failed: {}", strerror(err));
                    }
                    m_protocol_log_fs.flush();
                }
#endif

                cb({});
            });
    }

    virtual void set_option(imap_socket_opts::dump_stream_to_file) override {
        m_opt_dump_stream_to_file = true;
    }

    bool initialize(bool dump_stream_to_file) {
        if (dump_stream_to_file) {
            m_protocol_log_fs =
                std::ofstream{"imap_socket_dump.bin",
                              std::ios_base::out | std::ios_base::app | std::ios_base::binary};
            if (!m_protocol_log_fs) {
                auto err = errno;
                log_error("failed opening protocol log file: {}", ::strerror(err));
                // TODO: allow to override with env variable/library opt and make this optional.
                return false;
            }
        }
        return true;
    }

   private:
    asio::io_context& m_ctx;
    asio::ssl::context m_ssl_ctx;
    asio::ssl::stream<asio::ip::tcp::socket> m_socket;
    bool m_connected = false;
    asio::streambuf m_recv_buff;
    bool m_opt_dump_stream_to_file = false;
    std::ofstream m_protocol_log_fs;
    size_t m_bytes_received_total = 0;
};

}  // namespace

std::shared_ptr<imap_socket_t> make_imap_socket(asio::io_context& ctx) {
    auto inst = std::make_shared<imap_client_impl_t>(ctx);
    // FIXME: we have dynamic flag for this so this should be on demand created.
    if (!inst->initialize(true)) {
        log_error("imap socket initialization failed");
        return nullptr;
    }
    return inst;
}

void async_keep_receiving_lines_until(
    std::weak_ptr<imap_socket_t> socket_ptr,
    fu2::function<std::error_code(const imap_response_line_t& l)> p,
    async_callback<void> cb) {
    // TODO: what about the lifetime? what if socket gets deleted, this should rather be a member or
    // we should pass shared_ptr/weak_ptr to socket.

    auto socket = socket_ptr.lock();
    if (!socket) {
        cb(make_error_code(std::errc::owner_dead));
        return;
    }

    socket->async_receive_line([cb = std::move(cb), p = std::move(p), socket_ptr = socket](
                                   std::error_code ec, imap_response_line_t line) mutable {
        if (ec) {
            cb(ec);
            return;
        }

        auto predicate_ec = p(line);
        if (predicate_ec) {
            cb(predicate_ec);
        } else {
            async_keep_receiving_lines_until(socket_ptr, std::move(p), std::move(cb));
        }
    });
}

}  // namespace emailkit