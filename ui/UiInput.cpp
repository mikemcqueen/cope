////////////////////////////////////////////////////////////////////////////////
//
// UiInput.cpp
//
////////////////////////////////////////////////////////////////////////////////

#include "msvc_wall.h"
#include <format>
#include <stdexcept>
#include "UiInput.h"
#include "Rect.h"
#include "log.h"

namespace ui::input {

  void Pause(size_t delay)
  {
    if (delay > 0) {
      ::Sleep((DWORD)delay);
    }
  }
  
#pragma warning(disable:4505) // unreferenced function

bool
Click(
    HWND            hWnd,
    POINT           point,
    Mouse::Button_t Button /*= Mouse::Button::Left*/)
{
    MoveToAbsolute(hWnd, point);
    Pause(DefaultMoveClickDelay);
    return Click(Button);
}

////////////////////////////////////////////////////////////////////////////////

bool
Click(
    Mouse::Button_t Button /*= Mouse::Button::Left*/)
{
    MouseDown(Button);
    MouseUp(Button);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool
MoveToAbsolute(
    HWND hWnd,
    POINT point,
    size_t count /*= 1*/)
{
    ClientToScreen(hWnd, &point);
    Normalize(point);
    INPUT Input      = { 0 };
    Input.type       = INPUT_MOUSE;
    Input.mi.dwFlags = MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE;
    Input.mi.dx      = point.x;
    Input.mi.dy      = point.y;
    while (0 < count--)
    {
        INPUT copyInput = Input;
        Send(copyInput);
        Pause(DefaultMoveDelay);
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool
SendChars(
   const char* pText)
{
    for (int index = 0; L'\0' != pText[index]; ++index)
    {
        if (!SendChar(pText[index]))
        {
            LogError(L"Input_t::SendChars(): Could not send string (%S) char (%d)",
                     pText, pText[index]);
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool
SendChar(
    char ch,
    size_t count /*= 1*/,
    size_t delay /*= kDefaultSendcharDelay */)
{
    short vkScan = GetVkCode(ch);
    BYTE vkCode = LOBYTE(vkScan);
    if (vkInvalid != vkCode)
    {
        INPUT shiftDown = { 0 };
        shiftDown.type = INPUT_KEYBOARD;
        shiftDown.ki.wVk = VK_SHIFT;
        INPUT shiftUp = shiftDown;
        shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;

        bool shift = 0 != (HIBYTE(vkScan) & 0x01); // shift key
        while (0 < count--)
        {
            INPUT keyDown    = { 0 };
            keyDown.type       = INPUT_KEYBOARD;
            keyDown.ki.wVk     = vkCode;
            INPUT keyUp = keyDown;
            keyUp.ki.dwFlags = KEYEVENTF_KEYUP;
            if (shift)
            {
                Send(shiftDown);
                Pause(delay);
            }
            Send(keyDown);
            Pause(delay);
            Send(keyUp);
            Pause(delay);
            if (shift)
            {
                Send(shiftUp);
                Pause(delay);
            }
        }
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool
SendKey(
    WORD vkKey,
    bool keyUp /*= false */)
{
    INPUT key    = { 0 };
    key.type       = INPUT_KEYBOARD;
    key.ki.wVk     = vkKey;
    if (keyUp)
    {
        key.ki.dwFlags = KEYEVENTF_KEYUP;
    }
    Send(key);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

short
GetVkCode(
    char ch)
{
    short vkScan = vkInvalid;
    switch (ch)
    {
    case '\r':    // fall through
    case '\n':    vkScan = VK_RETURN; break;
//    case L'\\':    vkCode = VK_DIVIDE; break;
//    case L'\010':  vkCode = VK_BACK;   break;
//    case L'\027':  vkCode = VK_ESCAPE; break;
    default:
        vkScan = ::VkKeyScan(ch);
        break;
    }
    if (vkInvalid == LOBYTE(vkScan))
    {
        LogError(L"Unknown vk code for (%d)", ch);
        throw std::logic_error("Unknown vk code");
    }
    return vkScan;
}

////////////////////////////////////////////////////////////////////////////////

void
Normalize(
    POINT& Point)
{
    double ScreenWidth = ::GetSystemMetrics(SM_CXSCREEN) - 1.0;
    double ScreenHeight = ::GetSystemMetrics(SM_CYSCREEN) - 1.0;
    Point.x = int(Point.x * (65535.0 / ScreenWidth));
    Point.y = int(Point.y * (65535.0 / ScreenHeight));
}

////////////////////////////////////////////////////////////////////////////////

bool
MouseDown(
    Mouse::Button_t /*Button*/)
{
    INPUT Input      = { 0 };
    Input.type       = INPUT_MOUSE;
    Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    Send(Input);
    Pause(DefaultClickDelay);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool
MouseUp(
    Mouse::Button_t /*Button*/)
{
    INPUT Input      = { 0 };
    Input.type       = INPUT_MOUSE;
    Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    Send(Input);
    Pause(DefaultClickDelay);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool
Send(
    INPUT& input) 
{
    UINT EventCount = SendInput(1, &input, sizeof(INPUT));
    if (EventCount != 1)
    {
        LogError(L"Input_t::Send() Size(%d) Sent(%d) GLE(%d)",
                 1, EventCount, GetLastError());
        return false;
    }
    return true;
}

} // ui::input
