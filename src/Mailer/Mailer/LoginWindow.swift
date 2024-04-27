//
//  LoginWindow.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

class LoginWindow: NSWindow {
    override init(
        contentRect: NSRect, styleMask style: NSWindow.StyleMask,
        backing backingStoreType: NSWindow.BackingStoreType, defer flag: Bool
    ) {
        super.init(
            contentRect: contentRect, styleMask: [.fullSizeContentView], backing: .buffered,
            defer: true)
        isMovableByWindowBackground = true
    }
}
