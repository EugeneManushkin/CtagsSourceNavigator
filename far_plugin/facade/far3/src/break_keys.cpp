#include "text.h"

#include <far3/break_keys.h>
#include <far3/error.h>
#include <far3/plugin_sdk/api.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace
{
  using ScanType = decltype(::VkKeyScanExA(0, HKL()));
  using CharType = uint8_t;
  using Far3::KeyEvent;
  using Far3::UseLayouts;

  std::vector<HKL> GetKeyboardLayouts()
  {
    auto sz = GetKeyboardLayoutList(0, nullptr);
    if (!sz)
      throw Far3::Error(MFailedGetKeyboardLayoutListSize, "GetLastError", std::to_string(GetLastError()));

    std::vector<HKL> result(sz);
    if (!GetKeyboardLayoutList(sz, &result[0]))
      throw Far3::Error(MFailedGetKeyboardLayoutList, "GetLastError", std::to_string(GetLastError()));

    return result;
  }

  bool Supports(HKL layout, std::string const& characters)
  {
    auto not_scanned = [layout](char const& c){return ::VkKeyScanExA(static_cast<CharType>(c), layout) == -1;};
    return std::find_if(characters.cbegin(), characters.cend(), not_scanned) == characters.cend();
  }

  HKL GetLatinKeyboardLayout(std::vector<HKL> const& layouts)
  {
    std::string const latins = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    auto found = std::find_if(layouts.cbegin(), layouts.cend(), [&latins](HKL layout){return Supports(layout, latins);});
    if (found == layouts.cend())
      throw Far3::Error(MNoLatinKeyboardLayout, "latin_keys", latins);

    return *found;
  }

  DWORD WINAPI GetKeyboardLayoutThread(void* param)
  {
    auto result = static_cast<HKL*>(param);
    *result = ::GetKeyboardLayout(0);
    return 0;
  }

  HKL GetCurrentKeyboardLayout()
  {
    HKL result = 0;
    DWORD tid = 0;
    auto h = CreateThread(nullptr, 0, GetKeyboardLayoutThread, &result, 0, &tid);
    if (h != INVALID_HANDLE_VALUE)
    {
      WaitForSingleObject(h, INFINITE);
      CloseHandle(h); 
    }
    return result;
  }
  
  DWORD ToFarControlState(WORD controlState)
  {
    switch (controlState)
    {
    case 1:
      return SHIFT_PRESSED;
    case 2:
      return LEFT_CTRL_PRESSED;
    case 4:
      return LEFT_ALT_PRESSED;
    }

    return 0;
  }

  ::FarKey ToFarKey(ScanType scan)
  {
    WORD key = scan & 0xff;
    WORD controlState = (scan & 0xff00) >> 8;
    return {key, ToFarControlState(controlState)};
  }

  class BreakKeysImpl : public Far3::BreakKeys
  {
  public:
    BreakKeysImpl(std::vector<KeyEvent> const& events, UseLayouts useLayouts);
    ::FarKey const* GetBreakKeys() const override;
    KeyEvent GetEvent(int index) const override;
    int GetChar(int index) const override;

  private:
    using IndexType = int;
    using IndexesToCharsType = std::unordered_map<IndexType, CharType>;

    void PutChars(std::vector<HKL>&& layouts);
    void PutEvents(std::vector<KeyEvent> const& events);
    void PutEvent(::FarKey&& key, KeyEvent event, std::vector<KeyEvent> const& allowedEvents);

  private:
    UseLayouts UsedLayout;
    std::vector<::FarKey> Keys;
    std::unordered_map<HKL, IndexesToCharsType> Locales;
    std::unordered_map<IndexType, KeyEvent> Events;

  private:
    struct LocalesBuilder
    {
      LocalesBuilder(std::vector<HKL>&& layouts);
      void PutChar(CharType c, BreakKeysImpl& self);

    private:
      std::vector<HKL> Layouts;
      std::unordered_map<ScanType, IndexType> ScansToKeys;
    };
  }; 

  BreakKeysImpl::BreakKeysImpl(std::vector<KeyEvent> const& events, UseLayouts useLayouts)
    : UsedLayout(useLayouts)
  {
    if (UsedLayout != UseLayouts::None)
    {
      auto layouts = GetKeyboardLayouts();
      auto latinLayout = GetLatinKeyboardLayout(layouts);
      PutChars(UsedLayout == UseLayouts::Latin ? std::vector<HKL>{latinLayout} : std::move(layouts));
    }

    PutEvents(events);
    if (Keys.empty())
      throw std::logic_error("No break keys registered");

    Keys.push_back(::FarKey());
  }

  ::FarKey const* BreakKeysImpl::GetBreakKeys() const
  {
    return Keys.data();
  }

  KeyEvent BreakKeysImpl::GetEvent(int index) const
  {
    auto found = Events.find(index);
    return found != Events.end() ? found->second : KeyEvent::NoEvent;
  }

  int BreakKeysImpl::GetChar(int index) const
  {
    auto locale = UsedLayout == UseLayouts::All ? Locales.find(GetCurrentKeyboardLayout()) : Locales.begin();
    if (locale == Locales.end())
      return Far3::BreakKeys::InvalidChar;

    auto const found = locale->second.find(index);
    return found != locale->second.end() ? found->second : Far3::BreakKeys::InvalidChar;
  }

  void BreakKeysImpl::PutChars(std::vector<HKL>&& layouts)
  {
    LocalesBuilder builder(std::move(layouts));
    for (int c = std::numeric_limits<CharType>::min(); c <= std::numeric_limits<CharType>::max(); ++c)
    {
      if (::IsCharAlphaA(static_cast<CharType>(c)))
        builder.PutChar(c, *this);
    }

    std::string const specialChars = "1234567890!@#$%^&*()=+-_,.<>;:/?~[]\\|'\"";
    for (auto c : specialChars)
    {
      builder.PutChar(c, *this);
    }
  }

  void BreakKeysImpl::PutEvents(std::vector<KeyEvent> const& events)
  {
    PutEvent({VK_TAB}, KeyEvent::Tab, events);
    PutEvent({VK_BACK}, KeyEvent::Backspace, events);
    PutEvent({VK_F4}, KeyEvent::F4, events);
    PutEvent({VK_INSERT, LEFT_CTRL_PRESSED}, KeyEvent::CtrlC, events);
    PutEvent({VK_INSERT, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlC, events);
    PutEvent({0x43, LEFT_CTRL_PRESSED}, KeyEvent::CtrlC, events);
    PutEvent({0x43, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlC, events);
    PutEvent({VK_INSERT, SHIFT_PRESSED}, KeyEvent::CtrlV, events);
    PutEvent({0x56, LEFT_CTRL_PRESSED}, KeyEvent::CtrlV, events);
    PutEvent({0x56, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlV, events);
    PutEvent({0x52, LEFT_CTRL_PRESSED}, KeyEvent::CtrlR, events);
    PutEvent({0x52, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlR, events);
    PutEvent({0x5A, LEFT_CTRL_PRESSED}, KeyEvent::CtrlZ, events);
    PutEvent({0x5A, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlZ, events);
    PutEvent({VK_DELETE, LEFT_CTRL_PRESSED}, KeyEvent::CtrlDel, events);
    PutEvent({VK_DELETE, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlDel, events);
    PutEvent({VK_RETURN, LEFT_CTRL_PRESSED}, KeyEvent::CtrlEnter, events);
    PutEvent({VK_RETURN, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlEnter, events);
  }

  void BreakKeysImpl::PutEvent(::FarKey&& key, KeyEvent event, std::vector<KeyEvent> const& allowedEvents)
  {
    if (std::find(allowedEvents.begin(), allowedEvents.end(), event) == allowedEvents.end())
      return;

    Events.emplace(static_cast<int>(Keys.size()), event);
    Keys.push_back(std::move(key));
  }

  BreakKeysImpl::LocalesBuilder::LocalesBuilder(std::vector<HKL>&& layouts)
    : Layouts(std::move(layouts))
  {
  }

  void BreakKeysImpl::LocalesBuilder::PutChar(CharType c, BreakKeysImpl& self)
  {
    for (auto layout : Layouts)
    {
      auto scan = ::VkKeyScanExA(c, layout);
      if (scan == -1)
        continue;
      
      auto inserted = ScansToKeys.emplace(scan, static_cast<int>(self.Keys.size()));
      self.Locales[layout].emplace(inserted.first->second, c);
      if (inserted.second)
        self.Keys.push_back(ToFarKey(scan));
    }
  }
}

namespace Far3
{
  std::unique_ptr<BreakKeys> BreakKeys::Create(std::vector<KeyEvent> const& events, UseLayouts useLayouts)
  {
    return std::unique_ptr<BreakKeys>(new BreakKeysImpl(events, useLayouts));
  }
}
