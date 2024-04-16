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
    void auth_done(std::error_code ec) override {
        assert(auth_done_fn_);
        auth_done_fn_(ec);
    }
    void tree_about_to_change() override {
        assert(tree_about_to_change_fn_);
        tree_about_to_change_fn_();
    }
    void tree_model_changed() override {
        assert(tree_model_changed_fn_);
        tree_model_changed_fn_();
    }

    // This function must be execute fb in the UI thread. This supposedly is the only place where it
    // is safe to update the model.
    void update_state(std::function<void()> fn) override {
        assert(update_state_fn_);
        update_state_fn_(std::move(fn));
    }

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
    std::function<void(std::error_code)> auth_done_fn_;
    std::function<void()> tree_about_to_change_fn_;
    std::function<void()> tree_model_changed_fn_;
    std::function<void(std::function<void()>)>
        update_state_fn_;  // TODO: consider renaming to dispath_ui(function<void()>)
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
                dispatch_async(dispatch_get_main_queue(), ^{
                    self.authInitiatedBlock([NSString stringWithUTF8String:uri.c_str()]);
                });
            } else {
                log_warning("No authInitiatedBlock block");
            }
        };
        
        mailer_with_listener_->auth_done_fn_ = [self](std::error_code ec) {
            if (self.authDoneBlock) {
                log_debug("Calling authDoneBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                    self.authDoneBlock(ec? FALSE : true);
                });
            } else {
                log_warning("No authDoneBlock block");
            }
        };
        
        mailer_with_listener_->tree_about_to_change_fn_ = [self]() {
            if (self.treeAboutToChangeBlock) {
                log_debug("Calling treeAboutToChangeBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                    self.treeAboutToChangeBlock();
                });
            } else {
                log_warning("No treeAboutToChangeBlock block");
            }
        };
        
        mailer_with_listener_->tree_model_changed_fn_ = [self]() {
            if (self.treeModelChangedBlock) {
                log_debug("Calling treeModelChangedBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                    self.treeModelChangedBlock();
                });
            } else {
                log_warning("No treeModelChangedBlock block");
            }
        };

        // TODO: it looks like update_state_fn (aka dispatch_ui) is not a callbacks but a depdency.
        // Tehcnically it can be among of callabcks but it should better be in separate pack if we
        // want to have descrnt API for C++ core.
        mailer_with_listener_->update_state_fn_ = [self](std::function<void()> fn) {
            dispatch_async(dispatch_get_main_queue(), ^{
              fn();
            });
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

- (void)asyncRunWithCompletionBlock:(void (^)(BOOL))block {
    mailer_with_listener_->mailer_instance_->async_run([block](std::error_code ec) {
        if (ec) {
            log_error("failed running mailer instance: {}", ec);
            block(FALSE);
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
          block(TRUE);
        });
    });
}
@end
