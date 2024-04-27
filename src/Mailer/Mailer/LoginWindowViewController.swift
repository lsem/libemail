//
//  LoginWindowViewController.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

//
//
// AppDelegate creates login window.
// Login window does nothing except controls its views so it just waits until user clock login.
// Corresponding delegate asks AppDelegate to handle this.
// We handle this by running different stuff with core instance.
// During manipulation with log in we use window only for presenting our results (how?).
// We can ask window to present Success dialog, or show failure. But what about having UI
// for some process, e.g. we want to test connection and it has progress bug.
// How it supposed to drain data?
// Alternatively, we can have app as singleton and allow everyone touch core from everywhere.
// But this may create problems. E.g. our loops are going to retain links to the app.

protocol LoginWindowViewControllerDelegate: AnyObject {
    func authDoneWithResult(success: Bool, creds: Credentials?)
}

class LoginWindowViewController: NSViewController {
    weak var delegate: LoginWindowViewControllerDelegate?
    weak var coreAppDelegate: CoreApplicationLoginDelegate?

    @IBOutlet weak var usernameTextField: NSTextField!
    @IBOutlet weak var passwordTextField: NSTextField!
    @IBOutlet weak var imapServerHostTextField: NSTextField!
    @IBOutlet weak var imapServerPortTextField: NSTextField!

    override func viewDidLoad() {
        super.viewDidLoad()
    }

    @IBAction func googleButtonAction(_ sender: Any) {
        print("LoginWindowViewController: google action")
        processGoogleAuthAction()
    }

    @IBAction func hotmailButtonAction(_ sender: Any) {
        print("LoginWindowViewController: hotmail action")
    }

    @IBAction func plainIMAPLoginAction(_ sender: Any) {
        print("LoginWindowViewController: plain IMAP action")
    }

    func processGoogleAuthAction() {
        print("calling asyncRequestGmailAuth")
        self.coreAppDelegate?.asyncRequestGmailAuth { succeded, uri in
            if !succeded {
                print("ERROR: asyncRequestGmailAuth failed")
                // TODO: ...
                self.delegate?.authDoneWithResult(success: false, creds: nil)
                return
            }

            print("asyncRequestGmailAuth succeeded")

            // TODO: use completion handler instead of assuming it is always successful
            NSWorkspace.shared.open(URL(string: uri!)!)

            self.coreAppDelegate?.asyncWaitAuthDone { succeded, creds in
                if !succeded {
                    print("ERROR: asyncWaitAuthDone failed")
                    // TODO: ...
                    self.delegate?.authDoneWithResult(success: false, creds: nil)
                    return
                }

                NSRunningApplication.current.activate(options: [
                    .activateAllWindows, .activateIgnoringOtherApps,
                ])

                self.coreAppDelegate?.asyncTestCreds(creds!) { succeded in
                    if !succeded {
                        print("ERROR: asyncTestCreds failed")
                        // TODO: ... Showing page and give a change to try again?
                        // what if there is not Internet?
                        self.delegate?.authDoneWithResult(success: false, creds: nil)
                        return
                    }

                    self.delegate?.authDoneWithResult(success: true, creds: creds)
                }
            }
        }
    }

}
