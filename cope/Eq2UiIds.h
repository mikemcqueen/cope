#pragma once

#include "uitypes.h"

using namespace std::literals;

namespace eq2::broker {

  constexpr auto First = 100;

  namespace sell_tab {
    namespace window {
      constexpr auto id{ "broker_sell_tab"sv };
    }

    namespace widget::id {
      enum : ui::WidgetId_t {
        SetPriceButton = First,
        ListItemButton,
        SearchButton,
        RemoveItemButton,
      };
    } // widget::id
  } // namespace sell_tab

  namespace set_price_popup {
    namespace window {
      constexpr auto id{ "set_price_popup"sv };
    }

    namespace widget::id {
      enum : ui::WidgetId_t {
        OneButton = First,
        TwoButton,
        ThreeButton,
        FourButton,
        FiveButton,
        SixButton,      //5
        SevenButton,
        EightButton,
        NineButton,
        ZeroButton,
        OkButton,       //10
        ClearButton,
        PlatinumButton,
        GoldButton,
        SilverButton,
        CopperButton,   //15
        PriceText
      };
    } // namespace widget::id
  } // namespace set_price_popup
} // namespace eq2::broker