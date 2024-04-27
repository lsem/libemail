//
//  AppDelegate.swift
//  Mailer
//
//  Created by semkiv on 30.03.2024.
//

import Cocoa

// The task for today is to really finalize decision on how state changes are going to be handled by Cocoa part.
// We want to have nicely placed and functional windows for Main window and for Login window. And whenever app decices that it needs to have login, they should be changed. We want to have a button telling the app to invalidate credentials, remove them and close current connection as if it can happen in the production.

// On UI preservation: https://bignerdranch.com/blog/cocoa-ui-preservation-yall/


@main
class AppDelegate: NSObject, NSApplicationDelegate, LoginViewControllerDelegate {
    var core: MailerAppCore? = nil
    
    lazy var mainWindowController = MainWindowController()
    lazy var loginWindowController = LoginWindowController()
    
    // LoginViewControllerDelegate -- begin
    func mailerInstance() -> MailerAppCore {
        return self.core!
    }
    func loginGmailClicked() {
        print("loginGmailClicked")
    }
    // LoginViewControllerDelegate -- end

    func loginWindowFinished() {
        //        let mainWindowVC = createControllerOrDie(byID: "MainWindow")
        //        closeCurrentMainWindow()
        //        presentInNewMainWindow(viewController: mainWindowVC!)
    }

    func closeCurrentMainWindow() {
        if NSApplication.shared.mainWindow == nil {
            print("No Main Window")
        }
        NSApplication.shared.mainWindow?.close()
    }

    func presentInNewMainWindow(viewController: NSViewController) {
        let window = NSWindow(contentViewController: viewController)


        let c = Credentials()

        window.styleMask = [.closable]
        window.isMovableByWindowBackground = true

        var rect = window.contentRect(forFrameRect: window.frame)
        // TODO: calculate perfectly centered window.
        rect.size = .init(width: 1000, height: 600)
        var frame = window.frameRect(forContentRect: rect)
        frame = frame.offsetBy(dx: -300, dy: -300)
        window.setFrame(frame, display: true, animate: true)

        window.makeKeyAndOrderFront(self)
        window.becomeMain()
        window.makeMain()
        let windowVC = NSWindowController(window: window)

        windowVC.showWindow(self)
    }

    func createControllerOrDie(byID: String) -> NSViewController! {
        if let mainStoryboard = NSStoryboard.main {
            if let windowNS = mainStoryboard.instantiateController(withIdentifier: byID)
                as? NSViewController
            {
                return windowNS
            } else {
                terminateApplicationCausedByCriticalError(
                    message: "Cannot instantiate controller with name \(byID)")
            }
        } else {
            terminateApplicationCausedByCriticalError(
                message: "Cannot access main storyboard of the app")
        }
        return nil
    }

    func coreCallback__stateChanged(_ s: ApplicationState) {
        print("APP/CORE: application state changed to \(s)")
        if s == .valueLoginRequired {
        } else if s == .valueIMAPEstablished {
        }
    }
    

    func coreCallback__AuthInitiated(_ uri: String) {
        print("APP/CORE: Core requested authentication, there URI is: \(uri)")
    }

    func coreCallback__treeAboutToChange() {
        print("APP/CORE: Tree about to change")
    }
    func coreCallback__treeModelChanged() {
        print("APP/CORE: Tree Model Changed")
    }

    func redirectLogsToFile() {
        let documentsPaths = NSSearchPathForDirectoriesInDomains(
            .documentDirectory, .userDomainMask, true)
        print("documentsPath: \(documentsPaths)")
        if documentsPaths.isEmpty {
            print("ERROR: documents directory not available")
            return
        }
        let path = (documentsPaths[0] as NSString).appendingPathComponent("console.log")
        print("redirecting to path: \(path)")
        freopen(path.cString(using: String.Encoding.utf8)!, "a+", stderr)
        freopen(path.cString(using: String.Encoding.utf8)!, "a+", stdout)
    }

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // TODO: decide if we need first open MainWindow and only then create and start the app.
        // Alternatively we can create some sort of SplashWindow for this short period of time but this is not a modern way. More modern way would be to present Main window but have some kind of mode in it which is disabled and greyed-out.
        self.mainWindowController.showWindow(nil)
        self.mainWindowController.window?.center()
        self.loginWindowController.showWindow(nil)
        self.loginWindowController.window?.center()
        

        self.core = MailerAppCore()

        if let core = self.core {

            core.stateChangedBlock = { state in
                self.coreCallback__stateChanged(state)
            }
            core.authInitiatedBlock = { uri in
                self.coreCallback__AuthInitiated(uri!)
            }
            core.authDoneBlock = { succeeded, creds in
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
                    print("core is ruunning")
                } else {
                    print("failed running mailer core")
                    self.terminateApplicationCausedByCriticalError(
                        message: "Failed running Core application")
                }
            })
        } else {
            terminateApplicationCausedByCriticalError(
                message: "Failed creating an instance of application core")
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
