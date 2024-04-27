//
//  LoginWindowController.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

protocol LoginWindowControllerDelegate: AnyObject {
    func authDoneWithResult(_ success: Bool, _ creds: Credentials?)
}

// Provides access to core functionality responsible for login function. Entire login will be using this interface to communicate with core.
protocol CoreApplicationLoginDelegate: AnyObject {
    func asyncRequestGmailAuth(_ completionCallback: @escaping (Bool, String?) -> Void)
    func asyncWaitAuthDone(_ completionCallback: @escaping (Bool, Credentials?) -> Void)
    func asyncTestCreds(_ creds: Credentials, _ completionCallback: @escaping (Bool) -> Void)
    func asyncAcceptCreds(_ creds: Credentials, _ completionCallback: @escaping (Bool) -> Void)
}

class LoginWindowController: NSWindowController, NSWindowDelegate, LoginWindowViewControllerDelegate
{
    weak var delegate: LoginWindowControllerDelegate?
    weak var coreAppLoginDelegate: CoreApplicationLoginDelegate?

    // MARK: LoginWindowViewControllerDelegate
    func authDoneWithResult(success: Bool, creds: Credentials?) {
        self.tabVC.selectedTabViewItemIndex = 1
        delegate?.authDoneWithResult(success, creds)
    }

    lazy var tabVC: NSTabViewController = {
        let mainPageLoginVC = LoginWindowViewController()
        mainPageLoginVC.coreAppDelegate = self.coreAppLoginDelegate
        mainPageLoginVC.delegate = self

        // Fix strange issue with incorrect size (500x500) of the window
        mainPageLoginVC.preferredContentSize = mainPageLoginVC.view.bounds.size

        let outcomeVC = LoginViewController_Outcome()
        outcomeVC.preferredContentSize = mainPageLoginVC.view.bounds.size

        let tabVC = NSTabViewController()
        tabVC.addTabViewItem(NSTabViewItem(viewController: mainPageLoginVC))
        tabVC.addTabViewItem(NSTabViewItem(viewController: outcomeVC))
        tabVC.transitionOptions = .slideForward
        tabVC.tabStyle = .unspecified
        tabVC.view.frame = mainPageLoginVC.view.bounds

        return tabVC
    }()

    convenience init() {
        self.init(windowNibName: "LoginWindowController")
    }

    override func windowDidLoad() {
        super.windowDidLoad()
        self.contentViewController = tabVC
    }
}
