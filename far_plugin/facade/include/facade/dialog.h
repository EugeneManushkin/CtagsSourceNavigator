#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Facade
{
  class DialogController
  {
  public:
    virtual ~DialogController() = default;
    virtual std::string GetValue(std::string const& id) const = 0;
    virtual void SetValue(std::string const& id, std::string const& value) const = 0;
    virtual void SetEnabled(std::string const& id, bool enabled) const = 0;
  };

  class Dialog
  {
  public:
    using Callback = std::function<void(DialogController const& controller)>;
    static std::unique_ptr<Dialog> Create(int width);
    virtual ~Dialog() = default;
    virtual Dialog& SetTitle(int textID, std::string const& id = "") = 0;
    virtual Dialog& SetTitle(std::string const& text, std::string const& id = "") = 0;
    virtual Dialog& AddCaption(int textID, std::string const& id = "", bool enabled = true) = 0;
    virtual Dialog& AddEditbox(std::string const& value, std::string const& id = "", bool enabled = true, Callback cb = Callback()) = 0;
    virtual Dialog& AddCheckbox(int textID, std::string const& value, std::string const& id = "", bool enabled = true, Callback cb = Callback()) = 0;
    virtual Dialog& AddButton(int textID, std::string const& id = "", bool default = false, bool enabled = true, Callback cb = Callback()) = 0;
    virtual Dialog& AddSeparator() = 0;
    virtual Dialog& SetOnIdle(Callback cb) = 0;
    virtual std::unordered_map<std::string, std::string> Run() = 0;
  };
}
