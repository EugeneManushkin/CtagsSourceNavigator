#pragma once

#include <functional>
#include <stdexcept>
#include <utility>

namespace Facade
{
  using ExceptionHandler = std::function<void(std::exception_ptr)>;

  template <typename Callable, typename ...Args>
  auto SafeCall(Callable call, ExceptionHandler handler, Args&& ...args) -> 
    typename std::enable_if<!std::is_void<decltype(call(std::forward<Args>(args)...))>::value,
                          std::pair<bool, decltype(call(std::forward<Args>(args)...))>>::type
  try
  {
    return std::make_pair(true, call(std::forward<Args>(args)...));
  }
  catch(...)
  {
    if (handler) handler(std::current_exception());
    return std::make_pair(false, decltype(call(std::forward<Args>(args)...))());
  }

  template <typename Callable, typename ...Args>
  auto SafeCall(Callable call, ExceptionHandler handler, Args&& ...args) ->
    typename std::enable_if<std::is_void<decltype(call(std::forward<Args>(args)...))>::value,
      std::pair<bool, int>>::type
  try
  {
    call(std::forward<Args>(args)...);
    return std::make_pair(true, 0);
  }
  catch(...)
  {
    if (handler) handler(std::current_exception());
    return std::make_pair(false, 0);
  }
}
