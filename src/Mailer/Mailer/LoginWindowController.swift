//
//  LoginWindowController.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

protocol LoginWindowControllerDelegate: AnyObject {
    func viewControllerCreated(instance: LoginWindowViewController)
}

class LoginWindowController: NSWindowController, NSWindowDelegate {
    weak var delegate: LoginWindowControllerDelegate?

    convenience init() {
        self.init(windowNibName: "LoginWindowController")
    }

    override func windowDidLoad() {
        super.windowDidLoad()

        // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
        let viewController = LoginWindowViewController()
        self.contentViewController = viewController
        delegate?.viewControllerCreated(instance: viewController)
    }
}
