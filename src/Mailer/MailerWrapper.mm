//
//  MailerWrapper.m
//  Mailer
//
//  Created by semkiv on 30.03.2024.
//

#import <Foundation/Foundation.h>

#include <vector>
#include <mailer_poc.hpp>
#include <thread>


void start() {
    auto mailer = mailer::make_mailer_poc();
    std::thread th {[mailer=std::move(mailer)](){
        mailer->stop_working_thread();
    }};
}
