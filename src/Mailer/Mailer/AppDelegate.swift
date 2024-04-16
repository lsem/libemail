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
    
    func coreCallback__AuthInitiated(_ uri: String) {
        print("APP/CORE: Core requested authentication, there URI is: \(uri)")
        NSWorkspace.shared.open(URL(string: uri)!)
        // TODO: change view controller!
    }
    
    func coreCallback__authDone(_ succeed: Bool) {
        print("APP/CORE: Auth done")
        // TODO: change view controller!
    }
    func coreCallback__treeAboutToChange() {
        print("APP/CORE: Tree about to change")
    }
    func coreCallback__treeModelChanged() {
        print("APP/CORE: Tree Model Changed")
    }
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application

        self.core = MailerAppCore()
        
        if let core = self.core {
            
            core.authInitiatedBlock = { uri in
                self.coreCallback__AuthInitiated(uri!)
            }
            core.authDoneBlock = { succeeded in
                self.coreCallback__authDone(succeeded)
            }
            core.treeAboutToChangeBlock = {
                self.coreCallback__treeAboutToChange()
            }
            core.treeModelChangedBlock = {
                self.coreCallback__treeModelChanged()
            }

            // TODO: implement more callbacks, especiall those that update UI.
            
            core.startEventLoop()
            core.asyncRun(completionBlock: { succeded in
                              if succeded {
                                  print("core is ruunning");
                              } else {
                                  print("failed running mailer core")
                                  self.terminateApplicationCausedByCriticalError(message: "Failed running Core application")
                              }
                          })
        } else {
            terminateApplicationCausedByCriticalError(message: "Failed creating an instance of application core")
        }
    }
    
    func terminateApplicationCausedByCriticalError(message: String) {
        let alert = NSAlert()
        alert.messageText = "\(message).\nThis is fatal error and application will be closed"
        alert.alertStyle = NSAlert.Style.critical
        alert.runModal()
        // TODO: deliver information to develoeprs. Ask user persmissions for sending this  (via email?).
        NSApplication.shared.terminate(nil)
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
        if let core = self.core {
            // TODO: here should be some timeout after which we terminate no matter what
            core.stopEventLoop()
        }
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }
}


