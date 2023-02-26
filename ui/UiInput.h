/////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2008-2009 Mike McQueen.  All rights reserved.
//
// UiInput.h
//
/////////////////////////////////////////////////////////////////////////////

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef Include_UIINPUT_H
#define Include_UIINPUT_H

#include "UiTypes.h"

class Rect_t;

namespace ui
{

namespace input
{

    inline static const BYTE   vkInvalid             = 0xff;
    inline static const size_t DefaultMoveDelay = 100;
    inline static const size_t DefaultClickDelay = 100;
    inline static const size_t DefaultSendCharDelay = 20;
    inline static const size_t DefaultMoveClickDelay = 250;

    bool
    Click(
        HWND            hWnd,
        POINT           point,
        Mouse::Button_t Button = Mouse::Button::Left);

    bool
    Click(
        Mouse::Button_t Button = Mouse::Button::Left);

    bool
    SendChars(
        const char* pText);

    bool
    SendChar(
        char ch,
        size_t  count = 1,
        size_t delay = DefaultSendCharDelay);

    bool
    SendKey(
        WORD vkKey,
        bool keyUp = false);

    bool
    MoveToAbsolute(
        HWND hWnd,
        POINT pt,
        size_t count = 1);

    bool
    MouseDown(
        Mouse::Button_t Button);

    bool
    MouseUp(
        Mouse::Button_t Button);

    bool
    Send(
        INPUT& input);


    void
    Normalize(
        POINT& Point);

    short
    GetVkCode(
        char ch);

};

} // Ui

#endif // Include_UIINPUT_H
