#include <facade/safe_call.h>
#include <gtest/gtest.h>

namespace
{
  class MockArgument
  {
    int Counter;

  public:
    MockArgument()
      : Counter(0)
    {
    }

    MockArgument(MockArgument const& arg)
      : Counter(arg.Counter + 1)
    {
    }

    MockArgument(MockArgument&& arg)
      : Counter(arg.Counter)
    {
    }

    int CopyCount() const
    {
      return Counter;
    }
  };

  class ExceptionClass
  {
  };

  void VoidThrows() { throw ExceptionClass(); };
  void VoidNotThrows() {};
  std::string NonvoidThrows() { throw ExceptionClass(); };
  std::string NonvoidNotThrows() { return "test value"; };
  std::string NonvoidArgsThrows(MockArgument const& a, MockArgument const& b) { throw ExceptionClass(); };
  std::string NonvoidArgsNotThrows(MockArgument const& a, MockArgument const& b) { return "test value"; };
}

namespace Tests
{
  class SafeCall : public ::testing::Test
  {
    std::exception_ptr Exception;

  protected:
    Facade::ExceptionHandler Handler()
    {
      return [this](std::exception_ptr e) { Exception = e; };
    }

    void Called()
    {
      if (Exception) std::rethrow_exception(Exception);
    }
  };

  TEST_F(SafeCall, ProtectsVoidFunction)
  {
    ASSERT_FALSE(Facade::SafeCall(VoidThrows, Handler()).first);
    ASSERT_THROW(Called(), ExceptionClass);
  }

  TEST_F(SafeCall, CallsVoidFunction)
  {
    ASSERT_TRUE(Facade::SafeCall(VoidNotThrows, Handler()).first);
    ASSERT_NO_THROW(Called());
  }

  TEST_F(SafeCall, ProtectsNonvoidFunction)
  {
    ASSERT_EQ(std::make_pair(false, decltype(NonvoidThrows())()), Facade::SafeCall(NonvoidThrows, Handler()));
    ASSERT_THROW(Called(), ExceptionClass);
  }

  TEST_F(SafeCall, CallsNonvoidFunction)
  {
    auto const expected = std::make_pair(true, NonvoidNotThrows());
    ASSERT_EQ(expected, Facade::SafeCall(NonvoidNotThrows, Handler()));
    ASSERT_NO_THROW(Called());
  }

  TEST_F(SafeCall, ProtectsFunctionWithArguments)
  {
    auto const expected = std::make_pair(false, decltype(NonvoidArgsThrows(MockArgument(), MockArgument()))());
    ASSERT_EQ(expected, Facade::SafeCall(NonvoidArgsThrows, Handler(), MockArgument(), MockArgument()));
    ASSERT_THROW(Called(), ExceptionClass);
  }

  TEST_F(SafeCall, CallsFunctionWithArguments)
  {
    auto const expected = std::make_pair(true, NonvoidArgsNotThrows(MockArgument(), MockArgument()));
    ASSERT_EQ(expected, Facade::SafeCall(NonvoidArgsNotThrows, Handler(), MockArgument(), MockArgument()));
    ASSERT_NO_THROW(Called());
  }

  TEST_F(SafeCall, ProtectsBindedFunction)
  {
    auto const expected = std::make_pair(false, decltype(NonvoidArgsThrows(MockArgument(), MockArgument()))());
    ASSERT_EQ(expected, Facade::SafeCall(std::bind(NonvoidArgsThrows, std::placeholders::_1, MockArgument()), Handler(), MockArgument()));
    ASSERT_THROW(Called(), ExceptionClass);
  }

  TEST_F(SafeCall, CallsBindedFunction)
  {
    auto const expected = std::make_pair(true, NonvoidArgsNotThrows(MockArgument(), MockArgument()));
    ASSERT_EQ(expected, Facade::SafeCall(std::bind(NonvoidArgsNotThrows, std::placeholders::_1, MockArgument()), Handler(), MockArgument()));
    ASSERT_NO_THROW(Called());
  }

  TEST_F(SafeCall, CopiesArgumentsToFunction)
  {
    int const CopiedOnce = 1;
    MockArgument arg;
    auto result = Facade::SafeCall([](MockArgument arg){ return arg.CopyCount(); }, Handler(), arg);
    ASSERT_EQ(std::make_pair(true, CopiedOnce), result);
  }

  TEST_F(SafeCall, MovesArgumentsToFunction)
  {
    int const CopiedZeroTimes = 0;
    MockArgument arg;
    auto result = Facade::SafeCall([](MockArgument arg){ return arg.CopyCount(); }, Handler(), std::move(arg));
    ASSERT_EQ(std::make_pair(true, CopiedZeroTimes), result);
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
