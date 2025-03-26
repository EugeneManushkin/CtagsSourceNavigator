#include <gtest/gtest.h>

#include <far3/break_keys.h>
#include <far3/plugin_sdk/api.h>

namespace Far3
{
  namespace Tests
  {
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
      ASSERT_NO_FATAL_FAILURE(TestReturnInvalid(*BreakKeys::Create(false)));
    }

    TEST(BreakKeysOnlyLatin, ReturnInvalid)
    {
      ASSERT_NO_FATAL_FAILURE(TestReturnInvalid(*BreakKeys::Create(true)));
    }

    struct BreakKeysTestParam
    {
      std::string Name;
      FarKey Key;
      int ExpectedChar;
      KeyEvent ExpectedEvent;
    };

    BreakKeysTestParam BreakKeysTestParams[] = {
        {"VK_0", {0x30, 0}, '0'}
      , {"VK_1", {0x31, 0}, '1'}
      , {"VK_2", {0x32, 0}, '2'}
      , {"VK_3", {0x33, 0}, '3'}
      , {"VK_4", {0x34, 0}, '4'}
      , {"VK_5", {0x35, 0}, '5'}
      , {"VK_6", {0x36, 0}, '6'}
      , {"VK_7", {0x37, 0}, '7'}
      , {"VK_8", {0x38, 0}, '8'}
      , {"VK_9", {0x39, 0}, '9'}
      , {"VK_a", {0x41, 0}, 'a'}
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
    };

    std::string BreakKeysTestParamName(const testing::TestParamInfo<BreakKeysTestParam>& info)
    {
      return info.param.Name;
    }

    struct BreakKeysP: ::testing::TestWithParam<BreakKeysTestParam> {};
    struct BreakKeysOnlyLatinP: ::testing::TestWithParam<BreakKeysTestParam> {};

    void TestGetChar(BreakKeys const& SUT, BreakKeysTestParam const& param)
    {
      auto index = Find(param.Key, SUT.GetBreakKeys());
      ASSERT_NE(-1, index);
      ASSERT_EQ(param.ExpectedEvent, SUT.GetEvent(index));
      ASSERT_EQ(param.ExpectedChar, SUT.GetChar(index));
    }

    TEST_P(BreakKeysP, GetChar)
    {
      ASSERT_NO_FATAL_FAILURE(TestGetChar(*BreakKeys::Create(false), GetParam()));
    }

    TEST_P(BreakKeysOnlyLatinP, GetChar)
    {
      ASSERT_NO_FATAL_FAILURE(TestGetChar(*BreakKeys::Create(true), GetParam()));
    }

    INSTANTIATE_TEST_CASE_P(, BreakKeysP, ::testing::ValuesIn(BreakKeysTestParams), BreakKeysTestParamName);
    INSTANTIATE_TEST_CASE_P(, BreakKeysOnlyLatinP, ::testing::ValuesIn(BreakKeysTestParams), BreakKeysTestParamName);
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
