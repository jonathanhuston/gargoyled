// Copyright (C) 2026 by Jonathan Huston.
//
// This file is part of Informed (Gargoyle fork with LLM-powered natural language input).
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "keychainmac.h"

#include <iostream>

#import <Cocoa/Cocoa.h>
#import <Security/Security.h>

static NSString *const kServiceName = @"com.informed.gargoyle";
static NSString *const kAccountName = @"ANTHROPIC_API_KEY";

// NSAlert intercepts key equivalents, so Cmd+V/C/X/A don't reach the
// accessory view's text field.  This subclass handles them directly.
@interface KeychainTextField : NSTextField
@end

@implementation KeychainTextField
- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if ([event modifierFlags] & NSEventModifierFlagCommand) {
        NSString *chars = [event charactersIgnoringModifiers];
        if ([chars isEqualToString:@"v"]) { [self.currentEditor paste:nil];     return YES; }
        if ([chars isEqualToString:@"c"]) { [self.currentEditor copy:nil];      return YES; }
        if ([chars isEqualToString:@"x"]) { [self.currentEditor cut:nil];       return YES; }
        if ([chars isEqualToString:@"a"]) { [self.currentEditor selectAll:nil]; return YES; }
    }
    return [super performKeyEquivalent:event];
}
@end

namespace garglk { namespace keychain {

std::string get_api_key()
{
    NSDictionary *query = @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: kServiceName,
        (__bridge id)kSecAttrAccount: kAccountName,
        (__bridge id)kSecReturnData:  @YES,
        (__bridge id)kSecMatchLimit:  (__bridge id)kSecMatchLimitOne,
    };

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if (status == errSecSuccess && result != nullptr) {
        NSData *data = (NSData *)result;
        NSString *key = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        std::string key_str = [key UTF8String];
        [key release];
        CFRelease(result);
        return key_str;
    }

    if (status != errSecItemNotFound) {
        std::cerr << "[Informed] Keychain read error: " << status << std::endl;
    }

    return "";
}

bool set_api_key(const std::string &key)
{
    NSData *keyData = [[NSString stringWithUTF8String:key.c_str()] dataUsingEncoding:NSUTF8StringEncoding];

    // First try to update an existing item
    NSDictionary *query = @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: kServiceName,
        (__bridge id)kSecAttrAccount: kAccountName,
    };

    NSDictionary *update = @{
        (__bridge id)kSecValueData: keyData,
    };

    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);

    if (status == errSecItemNotFound) {
        // Item doesn't exist yet, add it
        NSDictionary *addQuery = @{
            (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: kServiceName,
            (__bridge id)kSecAttrAccount: kAccountName,
            (__bridge id)kSecValueData:   keyData,
        };

        status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    }

    if (status != errSecSuccess) {
        std::cerr << "[Informed] Keychain write error: " << status << std::endl;
        return false;
    }

    std::cerr << "[Informed] API key stored in macOS Keychain." << std::endl;
    return true;
}

std::string prompt_for_api_key()
{
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Anthropic API Key";
    alert.informativeText = @"Enter your Anthropic API key to enable natural language input translation. "
                             "The key will be stored securely in your macOS Keychain.\n\n"
                             "Get a key at: console.anthropic.com";
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Skip"];
    alert.window.level = NSModalPanelWindowLevel;

    KeychainTextField *input = [[KeychainTextField alloc] initWithFrame:NSMakeRect(0, 0, 340, 24)];
    input.placeholderString = @"sk-ant-...";
    [alert setAccessoryView:input];

    // Focus the text field when the dialog appears
    [alert.window setInitialFirstResponder:input];

    NSModalResponse response = [alert runModal];
    std::string result;

    if (response == NSAlertFirstButtonReturn) {
        NSString *value = [input stringValue];
        if ([value length] > 0) {
            result = [value UTF8String];
            set_api_key(result);
        }
    }

    [input release];
    [alert release];

    return result;
}

}} // namespace garglk::keychain
