//
//  AppDelegate.swift
//  Mailer
//
//  Created by semkiv on 30.03.2024.
//

import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {
    var core : MailerAppCore? = nil
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application

        self.core = MailerAppCore()

        
        if let core = self.core {

            core.authInitiatedBlock = { uri in
                print("Core requested authentication, there URI is: \(uri)")
            }
            
            core.startEventLoop()
            core.asyncRun(completionBlock: { succeded in
                              if succeded {
                                  print("core is ruunning");
                              } else {
                                  print("failed running mailer core")
                              }
                          })
        } else {
            let alert = NSAlert()
            alert.messageText = "Failed creating an instance of application core. This is fatal error and application will be closed"
            alert.alertStyle = NSAlert.Style.critical
            alert.runModal()
            // TODO: deliver information to develoeprs. Ask user persmissions for sending this  (via email?).
            NSApplication.shared.terminate(nil)
        }

    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }


}


