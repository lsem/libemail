//
//  MailerAppCore.m
//  Mailer
//
//  Created by semkiv on 13.04.2024.
//

#include "MailerAppCore.h"

#include <mailer_poc.hpp>

class MailerGluedWithItsListener : public mailer::MailerPOCCallbacks {
   public:
    void auth_initiated(std::string uri) override {
        assert(auth_initiated_fn_);
        auth_initiated_fn_(uri);
    }
    void auth_done(std::error_code) override {}
    void tree_about_to_change() override {}
    void tree_model_changed() override {}

    // This function must be execute fb in the UI thread. This supposedly is the only place where it
    // is safe to update the model.
    void update_state(std::function<void()> fn) override {}

    static std::unique_ptr<MailerGluedWithItsListener> create_instance() {
        auto mailer_instance_ = mailer::make_mailer_poc();
        if (!mailer_instance_) {
            log_error("failed creating mailer poc instance");
            return nullptr;
        }

        auto instance = std::make_unique<MailerGluedWithItsListener>();

        mailer_instance_->set_callbacks_if(instance.get());
        instance->mailer_instance_ = std::move(mailer_instance_);

        return instance;
    }

    std::shared_ptr<mailer::MailerPOC> mailer_instance_;

    std::function<void(std::string)> auth_initiated_fn_;
    std::function<void()> auth_done_fn_;
    std::function<void()> tree_about_to_change_fn_;
    std::function<void()> tree_model_changed_fn_;
};

struct MailerPOCCallbacks {
   protected:
    ~MailerPOCCallbacks() = default;

   public:
    // Auth server is ready on URI. We can improve overall handling by first requesting reauth so
    // the client can prepare UI accordingly, then resolve callback and only then have server
    // started.
};

@interface MailerAppCore () {
    std::unique_ptr<MailerGluedWithItsListener> mailer_with_listener_;
}
@end

@implementation MailerAppCore

- (instancetype)init {
    self = [super init];
    if (self) {
        mailer_with_listener_ = MailerGluedWithItsListener::create_instance();
        if (!mailer_with_listener_) {
            log_error("failed creating mailer poc instance");
            // ARC will take care of deallocating self.
            return nil;
        }

        mailer_with_listener_->auth_initiated_fn_ = [self](std::string uri) {
            if (self.authInitiatedBlock) {
                log_debug("Calling authInitiatedBlock block");
                self.authInitiatedBlock([NSString stringWithUTF8String:uri.c_str()]);
            } else {
                log_warning("No authInitiatedBlock block");
            }
        };
    }
    return self;
}

- (void)startEventLoop {
    mailer_with_listener_->mailer_instance_->start_working_thread();
}

- (void)stopEventLoop {
    mailer_with_listener_->mailer_instance_->stop_working_thread();
}

//- (void)asyncRun {
- (void)asyncRunWithCompletionBlock:(void (^)(BOOL))block {
    mailer_with_listener_->mailer_instance_->async_run([block](std::error_code ec) {
        if (ec) {
            log_error("failed running mailer instance: {}", ec);
            block(FALSE);
            return;
        }
        block(TRUE);
    });
}
@end
