#pragma once

#include <facade/message.h>

namespace Facade
{
  namespace Internal
  {
    template <typename Callable>
    auto SafeCall(Callable call, decltype(call()) errorResult, bool& success) -> decltype(call())
    {
      try
      {
        success = true;
        return call();
      }
      catch (std::exception const& e)
      {
        Facade::ErrorMessage(e.what());
      }
    
      success = false;
      return errorResult;
    }

    template <typename Callable>
    auto SafeCall(Callable call, decltype(call()) errorResult) -> decltype(call())
    {
      bool success = true;
      return SafeCall(call, errorResult, success);
    }
    
    template <typename CallType>
    void SafeCall(CallType call)
    {
      SafeCall([call]() { call(); return 0; }, 0);
    }
  }
}
