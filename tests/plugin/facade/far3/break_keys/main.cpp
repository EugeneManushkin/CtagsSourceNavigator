#include <gtest/gtest.h>

#include <far3/break_keys.h>
#include <far3/plugin_sdk/api.h>

#include <stdexcept>

namespace Far3
{
  namespace Tests
  {
    std::vector<KeyEvent> const AllEvents = {
      KeyEvent::Tab
    , KeyEvent::Backspace
    , KeyEvent::F4
    , KeyEvent::CtrlC
    , KeyEvent::CtrlV
    , KeyEvent::CtrlP
    , KeyEvent::CtrlR
    , KeyEvent::CtrlZ
    , KeyEvent::CtrlDel
    , KeyEvent::CtrlEnter
    };

    int Count(::FarKey const* keys)
    {
      int i = 0;
      for (i = 0; !(keys[i].VirtualKeyCode == 0 && keys[i].ControlKeyState == 0); ++i);
      return i;
    }

    int Find(::FarKey const& key, ::FarKey const* keys)
    {
      for (int i = 0; !(keys[i].VirtualKeyCode == 0 && keys[i].ControlKeyState == 0); ++i)
        if (keys[i].VirtualKeyCode == key.VirtualKeyCode && keys[i].ControlKeyState == key.ControlKeyState)
          return i;

      return -1;
    }

    void TestReturnInvalid(BreakKeys const& SUT)
    {
      auto const keysCount = Count(SUT.GetBreakKeys());
      ASSERT_GT(keysCount, 0);
      ASSERT_EQ(KeyEvent::NoEvent, SUT.GetEvent(-1));
      ASSERT_EQ(KeyEvent::NoEvent, SUT.GetEvent(keysCount));
      ASSERT_EQ(BreakKeys::InvalidChar, SUT.GetChar(-1));
      ASSERT_EQ(BreakKeys::InvalidChar, SUT.GetChar(keysCount));
    }

    TEST(BreakKeys, ReturnInvalid)
    {
      ASSERT_NO_FATAL_FAILURE(TestReturnInvalid(*BreakKeys::Create(AllEvents, Far3::UseLayouts::All)));
    }

    TEST(BreakKeysOnlyLatin, ReturnInvalid)
    {
      ASSERT_NO_FATAL_FAILURE(TestReturnInvalid(*BreakKeys::Create(AllEvents, Far3::UseLayouts::Latin)));
    }

    TEST(BreakKeys, ThrowsIfNoKeys)
    {
      ASSERT_THROW(BreakKeys::Create({}, Far3::UseLayouts::None), std::logic_error);
    }

    struct BreakKeysTestParam
    {
      BreakKeysTestParam(std::string const& name, FarKey const& key, char expected)
        : Name(name)
        , Key(key)
        , ExpectedChar(static_cast<unsigned char>(expected))
        , ExpectedEvent(KeyEvent::NoEvent)
      {}

      BreakKeysTestParam(std::string const& name, FarKey const& key, KeyEvent expected)
        : Name(name)
        , Key(key)
        , ExpectedChar(-1)
        , ExpectedEvent(expected)
      {}

      std::string Name;
      FarKey Key;
      int ExpectedChar;
      KeyEvent ExpectedEvent;
    };

    std::ostream& operator << (std::ostream& stream, BreakKeysTestParam const& param)
    {
      return stream
        << "key:0x" << std::hex << param.Key.VirtualKeyCode << std::dec
        << ", ctrl:0x" << std::hex << param.Key.ControlKeyState << std::dec
        << ", char:" << param.ExpectedChar << "('" << static_cast<char>(static_cast<unsigned char>(param.ExpectedChar)) << "')"
        << ", event:" << static_cast<int>(param.ExpectedEvent);
    }

    enum NumKeys
    {
      VK_0 = 0x30,
      VK_1 = 0x31,
      VK_2 = 0x32,
      VK_3 = 0x33,
      VK_4 = 0x34,
      VK_5 = 0x35,
      VK_6 = 0x36,
      VK_7 = 0x37,
      VK_8 = 0x38,
      VK_9 = 0x39,
    };

    BreakKeysTestParam BreakKeysTestParams[] = {
        {"VK_a", {0x41, 0}, 'a'}
      , {"VK_b", {0x42, 0}, 'b'}
      , {"VK_c", {0x43, 0}, 'c'}
      , {"VK_d", {0x44, 0}, 'd'}
      , {"VK_e", {0x45, 0}, 'e'}
      , {"VK_f", {0x46, 0}, 'f'}
      , {"VK_g", {0x47, 0}, 'g'}
      , {"VK_h", {0x48, 0}, 'h'}
      , {"VK_i", {0x49, 0}, 'i'}
      , {"VK_j", {0x4A, 0}, 'j'}
      , {"VK_k", {0x4B, 0}, 'k'}
      , {"VK_l", {0x4C, 0}, 'l'}
      , {"VK_m", {0x4D, 0}, 'm'}
      , {"VK_n", {0x4E, 0}, 'n'}
      , {"VK_o", {0x4F, 0}, 'o'}
      , {"VK_p", {0x50, 0}, 'p'}
      , {"VK_q", {0x51, 0}, 'q'}
      , {"VK_r", {0x52, 0}, 'r'}
      , {"VK_s", {0x53, 0}, 's'}
      , {"VK_t", {0x54, 0}, 't'}
      , {"VK_u", {0x55, 0}, 'u'}
      , {"VK_v", {0x56, 0}, 'v'}
      , {"VK_w", {0x57, 0}, 'w'}
      , {"VK_x", {0x58, 0}, 'x'}
      , {"VK_y", {0x59, 0}, 'y'}
      , {"VK_z", {0x5A, 0}, 'z'}
      , {"VK_A", {0x41, SHIFT_PRESSED}, 'A'}
      , {"VK_B", {0x42, SHIFT_PRESSED}, 'B'}
      , {"VK_C", {0x43, SHIFT_PRESSED}, 'C'}
      , {"VK_D", {0x44, SHIFT_PRESSED}, 'D'}
      , {"VK_E", {0x45, SHIFT_PRESSED}, 'E'}
      , {"VK_F", {0x46, SHIFT_PRESSED}, 'F'}
      , {"VK_G", {0x47, SHIFT_PRESSED}, 'G'}
      , {"VK_H", {0x48, SHIFT_PRESSED}, 'H'}
      , {"VK_I", {0x49, SHIFT_PRESSED}, 'I'}
      , {"VK_J", {0x4A, SHIFT_PRESSED}, 'J'}
      , {"VK_K", {0x4B, SHIFT_PRESSED}, 'K'}
      , {"VK_L", {0x4C, SHIFT_PRESSED}, 'L'}
      , {"VK_M", {0x4D, SHIFT_PRESSED}, 'M'}
      , {"VK_N", {0x4E, SHIFT_PRESSED}, 'N'}
      , {"VK_O", {0x4F, SHIFT_PRESSED}, 'O'}
      , {"VK_P", {0x50, SHIFT_PRESSED}, 'P'}
      , {"VK_Q", {0x51, SHIFT_PRESSED}, 'Q'}
      , {"VK_R", {0x52, SHIFT_PRESSED}, 'R'}
      , {"VK_S", {0x53, SHIFT_PRESSED}, 'S'}
      , {"VK_T", {0x54, SHIFT_PRESSED}, 'T'}
      , {"VK_U", {0x55, SHIFT_PRESSED}, 'U'}
      , {"VK_V", {0x56, SHIFT_PRESSED}, 'V'}
      , {"VK_W", {0x57, SHIFT_PRESSED}, 'W'}
      , {"VK_X", {0x58, SHIFT_PRESSED}, 'X'}
      , {"VK_Y", {0x59, SHIFT_PRESSED}, 'Y'}
      , {"VK_Z", {0x5A, SHIFT_PRESSED}, 'Z'}

      , {"VK_0", {VK_0, 0}, '0'}
      , {"VK_1", {VK_1, 0}, '1'}
      , {"VK_2", {VK_2, 0}, '2'}
      , {"VK_3", {VK_3, 0}, '3'}
      , {"VK_4", {VK_4, 0}, '4'}
      , {"VK_5", {VK_5, 0}, '5'}
      , {"VK_6", {VK_6, 0}, '6'}
      , {"VK_7", {VK_7, 0}, '7'}
      , {"VK_8", {VK_8, 0}, '8'}
      , {"VK_9", {VK_9, 0}, '9'}
      , {"exclamation", {VK_1, SHIFT_PRESSED}, '!'}
      , {"at", {VK_2, SHIFT_PRESSED}, '@'}
      , {"sharp", {VK_3, SHIFT_PRESSED}, '#'}
      , {"dollar", {VK_4, SHIFT_PRESSED}, '$'}
      , {"percent", {VK_5, SHIFT_PRESSED}, '%'}
      , {"hat", {VK_6, SHIFT_PRESSED}, '^'}
      , {"ampersand", {VK_7, SHIFT_PRESSED}, '&'}
      , {"multiply", {VK_8, SHIFT_PRESSED}, '*'}
      , {"parenthesis_open", {VK_9, SHIFT_PRESSED}, '('}
      , {"parenthesis_close", {VK_0, SHIFT_PRESSED}, ')'}
      , {"equal", {VK_OEM_PLUS}, '='}
      , {"plus", {VK_OEM_PLUS, SHIFT_PRESSED}, '+'}
      , {"minus", {VK_OEM_MINUS}, '-'}
      , {"underscore", {VK_OEM_MINUS, SHIFT_PRESSED}, '_'}
      , {"comma", {VK_OEM_COMMA}, ','}
      , {"dot", {VK_OEM_PERIOD}, '.'}
      , {"less", {VK_OEM_COMMA, SHIFT_PRESSED}, '<'}
      , {"greater", {VK_OEM_PERIOD, SHIFT_PRESSED}, '>'}
      , {"semicolon", {VK_OEM_1}, ';'}
      , {"colon", {VK_OEM_1, SHIFT_PRESSED}, ':'}
      , {"slash", {VK_OEM_2}, '/'}
      , {"question", {VK_OEM_2, SHIFT_PRESSED}, '?'}
      , {"tilde", {VK_OEM_3, SHIFT_PRESSED}, '~'}
      , {"square_open", {VK_OEM_4}, '['}
      , {"square_close", {VK_OEM_6}, ']'}
      , {"backslash", {VK_OEM_5}, '\\'}
      , {"pipe_sign", {VK_OEM_5, SHIFT_PRESSED}, '|'}
      , {"quot", {VK_OEM_7}, '\''}
      , {"double_quot", {VK_OEM_7, SHIFT_PRESSED}, '"'}

      , {"Tab", {VK_TAB}, KeyEvent::Tab}
      , {"Backspace", {VK_BACK}, KeyEvent::Backspace}
      , {"F4", {VK_F4}, KeyEvent::F4}
      , {"LCtrlIns", {VK_INSERT, LEFT_CTRL_PRESSED}, KeyEvent::CtrlC}
      , {"RCtrlIns", {VK_INSERT, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlC}
      , {"LCtrlC", {0x43, LEFT_CTRL_PRESSED}, KeyEvent::CtrlC}
      , {"RCtrlC", {0x43, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlC}
      , {"ShiftIns", {VK_INSERT, SHIFT_PRESSED}, KeyEvent::CtrlV}
      , {"LCtrlV", {0x56, LEFT_CTRL_PRESSED}, KeyEvent::CtrlV}
      , {"RCtrlV", {0x56, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlV}
      , {"LCtrlP", {0x50, LEFT_CTRL_PRESSED}, KeyEvent::CtrlP}
      , {"RCtrlP", {0x50, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlP}
      , {"LCtrlR", {0x52, LEFT_CTRL_PRESSED}, KeyEvent::CtrlR}
      , {"RCtrlR", {0x52, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlR}
      , {"LCtrlZ", {0x5A, LEFT_CTRL_PRESSED}, KeyEvent::CtrlZ}
      , {"RCtrlZ", {0x5A, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlZ}
      , {"LCtrlDel", {VK_DELETE, LEFT_CTRL_PRESSED}, KeyEvent::CtrlDel}
      , {"RCtrlDel", {VK_DELETE, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlDel}
      , {"LCtrlEnter", {VK_RETURN, LEFT_CTRL_PRESSED}, KeyEvent::CtrlEnter}
      , {"RCtrlEnter", {VK_RETURN, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlEnter}
    };

    std::string BreakKeysTestParamName(const testing::TestParamInfo<BreakKeysTestParam>& info)
    {
      return info.param.Name;
    }

    struct BreakKeysP: ::testing::TestWithParam<BreakKeysTestParam> {};
    struct BreakKeysOnlyLatinP: ::testing::TestWithParam<BreakKeysTestParam> {};

    void TestGetChar(BreakKeys const& SUT, BreakKeysTestParam const& param)
    {
      auto const index = Find(param.Key, SUT.GetBreakKeys());
      ASSERT_NE(-1, index);
      ASSERT_EQ(param.ExpectedEvent, SUT.GetEvent(index));
      ASSERT_EQ(param.ExpectedChar, SUT.GetChar(index));
    }

    TEST_P(BreakKeysP, GetChar)
    {
      ASSERT_NO_FATAL_FAILURE(TestGetChar(*BreakKeys::Create(AllEvents, UseLayouts::All), GetParam()));
    }

    TEST_P(BreakKeysOnlyLatinP, GetChar)
    {
      ASSERT_NO_FATAL_FAILURE(TestGetChar(*BreakKeys::Create(AllEvents, UseLayouts::Latin), GetParam()));
    }

    INSTANTIATE_TEST_CASE_P(, BreakKeysP, ::testing::ValuesIn(BreakKeysTestParams), BreakKeysTestParamName);
    INSTANTIATE_TEST_CASE_P(, BreakKeysOnlyLatinP, ::testing::ValuesIn(BreakKeysTestParams), BreakKeysTestParamName);

    BreakKeysTestParam OnlyBreakKeysTestParams[] = {
      {"LCtrlIns", {VK_INSERT, LEFT_CTRL_PRESSED}, KeyEvent::CtrlC}
    , {"RCtrlIns", {VK_INSERT, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlC}
    , {"LCtrlC", {0x43, LEFT_CTRL_PRESSED}, KeyEvent::CtrlC}
    , {"RCtrlC", {0x43, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlC}
    , {"ShiftIns", {VK_INSERT, SHIFT_PRESSED}, KeyEvent::CtrlV}
    , {"LCtrlV", {0x56, LEFT_CTRL_PRESSED}, KeyEvent::CtrlV}
    , {"RCtrlV", {0x56, RIGHT_CTRL_PRESSED}, KeyEvent::CtrlV}
    };

    struct OnlyBreakKeysP: ::testing::TestWithParam<BreakKeysTestParam> {};

    TEST_P(OnlyBreakKeysP, GetKey)
    {
      auto const ExpectedBreakKeysCount = sizeof(OnlyBreakKeysTestParams)/sizeof(OnlyBreakKeysTestParams[0]);
      auto SUT = BreakKeys::Create({KeyEvent::CtrlC, KeyEvent::CtrlV}, UseLayouts::None);
      auto const index = Find(GetParam().Key, SUT->GetBreakKeys());
      ASSERT_NE(-1, index);
      ASSERT_EQ(BreakKeys::InvalidChar, SUT->GetChar(index));
      ASSERT_EQ(GetParam().ExpectedEvent, SUT->GetEvent(index));
      ASSERT_EQ(ExpectedBreakKeysCount, Count(SUT->GetBreakKeys()));
    }

    INSTANTIATE_TEST_CASE_P(, OnlyBreakKeysP, ::testing::ValuesIn(OnlyBreakKeysTestParams), BreakKeysTestParamName);
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
