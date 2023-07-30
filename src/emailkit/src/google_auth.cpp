#include "google_auth.hpp"
#include <fmt/format.h>
#include <emailkit/http_srv.hpp>
#include <emailkit/log.hpp>
#include <folly/folly_uri.hpp>
#include "uri_codec.hpp"

namespace emailkit {

namespace {
bool launch_system_browser(std::string uri) {
    // https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html

#if defined(__linux__)
    // for Linux:
    // https://www.baeldung.com/linux/open-url-in-default-browser
    log_error("launch_system_browser is not implemented");
    abort();  // not implemented
    return false;
#elif defined(__APPLE__)
    return std::system(fmt::format("open {}", uri).c_str()) == 0;
#elif defined(__WIN32)
    return std::system(fmt::format("start {}", uri).c_str()) == 0;
#endif
}

const std::string auth_page_template = R"(
<!doctype html>
<html lang="en"
<head> <meta charset="UTF-8"> </head>
<body>
    <a href="https://accounts.google.com/o/oauth2/auth?response_type=code&client_id={client_id}&scope={scope}&redirect_uri={redirect_uri}">OAuth 2.0 аутентифікація через Google</a>
</body>
</html>
)";

class google_auth_t_impl : public google_auth_t,
                           public std::enable_shared_from_this<google_auth_t_impl> {
   public:
    google_auth_t_impl(asio::io_context& ctx, std::string host, std::string port)
        : m_ctx(ctx), m_host(host), m_port(port) {}

    bool initialize() {
        m_srv = http_srv::make_http_srv(m_ctx, m_host, m_port);
        return m_srv != nullptr;
    }

    virtual void async_handle_auth(google_auth_app_creds_t app_creds,
                                   std::vector<std::string> scopes,
                                   async_callback<auth_data_t> cb) override {
        const auto scopes_encoded =
            emailkit::encode_uri_component(fmt::format("{}", fmt::join(scopes, " ")));
        log_debug("scopes_encoded: {}", scopes_encoded);

        // NOTE: redirect uri should be registred in google console in Credentials for the app.
        // othetwise one will got: 400/redirect_uri_mismatch.
        // TODO: make this possible to specify from parmater if we are building Kit here.
        const auto redirect_uri = emailkit::encode_uri_component(local_site_uri("/done"));

        const auto html_page =
            fmt::format(auth_page_template, fmt::arg("client_id", app_creds.client_id),
                        fmt::arg("scope", scopes_encoded), fmt::arg("redirect_uri", redirect_uri));

        log_debug("page: {}", html_page);

        m_srv->register_handler(
            "get", "/",
            [html_page](const http_srv::request& req, async_callback<http_srv::reply> cb) {
                http_srv::reply reply;
                reply.headers.emplace_back(http_srv::header{"Connection", "Close"});
                reply.content = html_page;
                cb({}, reply);
            });
        m_srv->register_handler(
            "get", "/done",
            [this, html_page, app_creds](const http_srv::request& req,
                                         async_callback<http_srv::reply> cb) {
                // TODO: don't use this!
                const auto complete_uri = local_site_uri("/done") + req.uri;

                log_warning("got something: {}", req);
                auto uri_or_err = folly::Uri::tryFromString(complete_uri);
                if (!uri_or_err) {
                    auto err = uri_or_err.takeError();
                    std::stringstream ss;
                    ss << err;
                    log_error("failed parsing uri: {}", ss.str());
                    log_debug("failed parsing uri: '{}'", complete_uri);
                    cb({}, http_srv::reply::stock_reply(http_srv::reply::internal_server_error));
                    return;
                }
                auto& uri = *uri_or_err;

                // TODO: don't know if we need to check if returned scope is requested one.
                std::string code;
                for (auto& [p, v] : uri.getQueryParams()) {
                    log_debug("param: {}: {}", p, v);
                    if (p == "code") {
                        code = v;
                    }
                }
                if (code.empty()) {
                    // something went wrong
                    log_error("unexpected response, code not found in URL from google: {}",
                              complete_uri);
                    cb({}, http_srv::reply::stock_reply(http_srv::reply::internal_server_error));
                    return;
                }

                // now when we have code we need to make post request.
                std::vector<std::string> token_request_params;
                token_request_params.emplace_back(fmt::format("code={}", code));
                token_request_params.emplace_back(fmt::format("client_id={}", app_creds.client_id));
                token_request_params.emplace_back(
                    fmt::format("client_secret={}", app_creds.client_secret));
                token_request_params.emplace_back(
                    fmt::format("redirect_uri={}", encode_uri_component(local_site_uri("/done2"))));
                token_request_params.emplace_back("grant_type=authorization_code");
                const auto token_request = fmt::format("{}", fmt::join(token_request_params, "&"));
                log_debug("token_request: {}", token_request);

                // TODO: consider having async operation here. For now we have sync one with 1s
                // timeout.

                cb({}, http_srv::reply::stock_reply(http_srv::reply::ok));
            });
        m_srv->register_handler(
            "get", "/favicon.ico",
            [](const http_srv::request& req, async_callback<http_srv::reply> cb) {
                // TODO:
                cb({}, http_srv::reply::stock_reply(http_srv::reply::not_found));
            });

        m_srv->start();
        // TODO: make sure we can really connect to immidiately after start() returned.

        launch_system_browser(local_site_uri("/"));
    }

    std::string local_site_uri(std::string resource) {
        return fmt::format("http://{}:{}{}", m_host, m_port, resource);
    }

   private:
    asio::io_context& m_ctx;
    std::string m_host;
    std::string m_port;
    shared_ptr<http_srv::http_srv_t> m_srv;
};
}  // namespace

shared_ptr<google_auth_t> make_google_auth(asio::io_context& ctx,
                                           std::string host,
                                           std::string port) {
    auto inst = std::make_shared<google_auth_t_impl>(ctx, host, port);
    if (!inst->initialize()) {
        return nullptr;
    }
    return inst;
}
}  // namespace emailkit
