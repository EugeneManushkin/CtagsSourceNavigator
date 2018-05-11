#include <gtest/gtest.h>

TEST(Tags, TrueIsTrue)
{
  ASSERT_TRUE(true);
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  RUN_ALL_TESTS();  
}