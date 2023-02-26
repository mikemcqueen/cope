/////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2008 Mike McQueen.  All rights reserved.
//
// UiTypes.h
//
/////////////////////////////////////////////////////////////////////////////

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef Include_UITYPES_H
#define Include_UITYPES_H

#include "UiWindowId.h"
#include "Rect.h"
#include "Flag_t.h"

namespace ui {
  using WidgetId_t = int;
  using widget_id_t = WidgetId_t;

  using WindowId_t = int;

  namespace Window {
    struct Handle_t {
      HWND       hWnd;
      WindowId_t WindowId;

      Handle_t(
        HWND       InitHwnd = nullptr,
        WindowId_t InitWindowId = 0/*Id::Unknown*/)
        :
        hWnd(InitHwnd),
        WindowId(InitWindowId)
      { }
    };
    class Base_t;

    namespace Flag
    {
      enum : Flag_t::value_type
      {
        VerticalScroll = 0x0001,
        HorizontalScroll = 0x0002,
        ScrollFlags = (VerticalScroll | HorizontalScroll),
      };
    }
  } // Window
  using Window_t = Window::Base_t;

  namespace Widget {
    struct Data_t
    {
      WidgetId_t             WidgetId;
      RelativeRect_t::Data_t RectData;
    };
  } // Widget

  namespace Mouse {
    using Button_t = int;
    namespace Button {
      enum : Button_t {
        Left,
        Middle,
        Right
      };
    }
  } // namespace Mouse

  namespace Scroll
  {
    namespace Bar
    {
      enum E : unsigned
      {
        Vertical,
        Horizontal,
      };
    }
    typedef Bar::E Bar_t;

    namespace Position
    {
      enum E : unsigned
      {
        Unknown,
        Top,
        Middle,
        Bottom
      };
    }
    typedef Position::E Position_t;

    namespace Direction
    {
      enum E : unsigned
      {
        Up,
        Down,
        Left,
        Right
      };
    }
    typedef Direction::E Direction_t;
  } // Scroll

} // ui

#endif Include_UITYPES_H
