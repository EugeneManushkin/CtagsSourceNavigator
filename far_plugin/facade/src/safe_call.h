#include <facade/message.h>

namespace FacadeInternal
{
  template <typename Callable>
  auto SafeCall(Callable call, decltype(call()) errorResult) -> decltype(call())
  {
    try
    {
      return call();
    }
    catch (std::exception const& e)
    {
      Facade::ErrorMessage(e.what());
    }
  
    return errorResult;
  }
  
  template <typename CallType>
  void SafeCall(CallType call)
  {
    SafeCall([call]() { call(); return 0; }, 0);
  }
}
