#import "Platform/Mac/MacInput.h"

namespace CloverPic::Platform::Mac {

uint32_t TranslateModifiers(NSEventModifierFlags flags) {
    uint32_t modifiers = 0;
    if ((flags & NSEventModifierFlagShift) != 0) modifiers |= Input::ModifierShift;
    // Core commands are written against a Control-based shortcut model, so the
    // Command key maps onto ModifierControl to keep Cmd+S / Cmd+Z native.
    if ((flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0) {
        modifiers |= Input::ModifierControl;
    }
    if ((flags & NSEventModifierFlagOption) != 0) modifiers |= Input::ModifierAlt;
    return modifiers;
}

uint32_t TranslateKeyCode(NSEvent* event) {
    switch (event.keyCode) {
        case 0x00: return 'A';
        case 0x0B: return 'B';
        case 0x08: return 'C';
        case 0x02: return 'D';
        case 0x0E: return 'E';
        case 0x03: return 'F';
        case 0x05: return 'G';
        case 0x04: return 'H';
        case 0x22: return 'I';
        case 0x26: return 'J';
        case 0x28: return 'K';
        case 0x25: return 'L';
        case 0x2E: return 'M';
        case 0x2D: return 'N';
        case 0x1F: return 'O';
        case 0x23: return 'P';
        case 0x0C: return 'Q';
        case 0x0F: return 'R';
        case 0x01: return 'S';
        case 0x11: return 'T';
        case 0x20: return 'U';
        case 0x09: return 'V';
        case 0x0D: return 'W';
        case 0x07: return 'X';
        case 0x10: return 'Y';
        case 0x06: return 'Z';
        case 0x12: return '1';
        case 0x13: return '2';
        case 0x14: return '3';
        case 0x15: return '4';
        case 0x17: return '5';
        case 0x16: return '6';
        case 0x1A: return '7';
        case 0x1C: return '8';
        case 0x19: return '9';
        case 0x1D: return '0';
        case 0x24: return 0x0D;  // Return
        case 0x30: return 0x09;  // Tab
        case 0x33: return 0x08;  // Backspace
        case 0x35: return 0x1B;  // Escape
        case 0x31: return 0x20;  // Space
        case 0x7B: return 0x25;  // Left
        case 0x7C: return 0x27;  // Right
        case 0x7D: return 0x28;  // Down
        case 0x7E: return 0x26;  // Up
        case 0x73: return 0x24;  // Home
        case 0x77: return 0x23;  // End
        default: break;
    }
    NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length > 0) {
        const unichar ch = [characters characterAtIndex:0];
        if (ch >= 'a' && ch <= 'z') return static_cast<uint32_t>(ch - 32);
        if (ch >= 'A' && ch <= 'Z') return ch;
        if (ch >= '0' && ch <= '9') return ch;
    }
    return 0;
}

Input::KeyEvent TranslateKeyEvent(NSEvent* event, Input::KeyAction action) {
    Input::KeyEvent key;
    key.action = action;
    key.keyCode = TranslateKeyCode(event);
    key.modifiers = TranslateModifiers(event.modifierFlags);
    key.isRepeat = event.isARepeat;
    return key;
}

} // namespace CloverPic::Platform::Mac
