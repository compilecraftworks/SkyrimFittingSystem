#pragma once
namespace sfs {
class InputManager {
public:
  static InputManager *GetSingleton() { static InputManager value; return &value; }
  void Flush() {}
};
}
