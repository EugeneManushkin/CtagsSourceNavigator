#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

namespace
{
  void Segfault()
  {
    int *ptr = nullptr;
    *ptr = 0;
  }

  int ToInt(std::string const& str)
  {
    int value = 0;
    if (sscanf(str.c_str(), "%d", &value) != 1)
      throw std::runtime_error("Invalid integer");

    return value;
  }
}

int main(int argc, char* argv[])
{
  if (argc != 3 && argc != 4)
    Segfault();

  if (argc == 4)
  {
    std::string left = argv[1];
    std::string right = argv[2];
    int exitCode = ToInt(argv[3]);
    std::replace(right.begin(), right.end(), '_', ' ');
    return left == right ? exitCode : 0;
  }

  int exitCode = ToInt(argv[1]);
  int timeout = ToInt(argv[2]);
  std::this_thread::sleep_for(std::chrono::seconds(timeout));
  return exitCode;
}
