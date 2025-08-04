#include <gtest/gtest.h>

#include <far3/guid.h>

#include <stdexcept>
#include <string>

namespace Far3
{
  namespace Tests
  {
    std::string const GuidString = "{2e34b611-3df1-463f-8711-74b0f21558a5}";
    ::GUID const ExpectedGuid = {0x2e34b611, 0x3df1, 0x463f, {0x87, 0x11, 0x74, 0xb0, 0xf2, 0x15, 0x58, 0xa5}};

    TEST(Guid, ReadGuid)
    {
      auto const SUT = Guid(GuidString.c_str());
      ASSERT_EQ(static_cast<::GUID>(SUT), ExpectedGuid);
    }

    TEST(Guid, ReadMixedCaseGuid)
    {
      auto const SUT = Guid("{2E34B611-3dF1-463f-8711-74B0f21558A5}");
      ASSERT_EQ(static_cast<::GUID>(SUT), ExpectedGuid);
    }

    TEST(Guid, ReadZeroGuid)
    {
      ::GUID const expected{};
      auto const SUT = Guid("{00000000-0000-0000-0000-000000000000}");
      ASSERT_EQ(static_cast<::GUID>(SUT), expected);
    }

    TEST(Guid, ReadGuidWithZeroes)
    {
      ::GUID const expected = {0x1, 0x2, 0x3, {0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0, 0xa}};
      auto const SUT = Guid("{00000001-0002-0003-0405-06070809000a}");
      ASSERT_EQ(static_cast<::GUID>(SUT), expected);
    }

    TEST(Guid, Throws)
    {
      ASSERT_THROW(Guid(""), std::invalid_argument);
      ASSERT_THROW(Guid("not-a-guid"), std::invalid_argument);
      ASSERT_THROW(Guid("{}"), std::invalid_argument);
      ASSERT_THROW(Guid("{2e34b611}"), std::invalid_argument);

      ASSERT_THROW(Guid("2e34b611-3df1-463f-8711-74b0f21558a5}"), std::invalid_argument);
      ASSERT_THROW(Guid("{2e34b611-3df1-463f-8711-74b0f21558a5"), std::invalid_argument);
      ASSERT_THROW(Guid("2e34b611-3df1-463f-8711-74b0f21558a5"), std::invalid_argument);
      ASSERT_THROW(Guid("{2e34b611-3df1-463f-8711-74b0f21558a5} "), std::invalid_argument);
      ASSERT_THROW(Guid(" {2e34b611-3df1-463f-8711-74b0f21558a5}"), std::invalid_argument);
      ASSERT_THROW(Guid(" {2e34b611-3df1-463f-8711-74b0f21558a5} "), std::invalid_argument);
      ASSERT_THROW(Guid("{2e34b611-3df1-463f-8711-74b0f21558a5} someth"), std::invalid_argument);
      ASSERT_THROW(Guid("someth {2e34b611-3df1-463f-8711-74b0f21558a5}"), std::invalid_argument);
      ASSERT_THROW(Guid("someth {2e34b611-3df1-463f-8711-74b0f21558a5} someth"), std::invalid_argument);

      for (size_t i = 0; i < GuidString.length(); ++i)
      {
        auto str = GuidString + " ";
        str.erase(i, 1);
        ASSERT_THROW(Guid(str.c_str()), std::invalid_argument) << "Guid string: '" << str << "'";
      }

      for (size_t i = 0; i < GuidString.length(); ++i)
      {
        auto str = GuidString;
        str[i] = 'z';
        ASSERT_THROW(Guid(str.c_str()), std::invalid_argument) << "Guid string: '" << str << "'";
      }
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
