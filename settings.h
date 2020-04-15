#pragma once

#include <memory>
#include <string>
#include <functional>

#include <yaml.h>

struct Settings {
  std::string identify;

  template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) {
    return f(settings.identify, node["identify"]);
  }
};

inline Settings settings;
extern void InitHttp();
extern void OnExit();
extern std::string Fetch(wchar_t const *path);
extern void AddTask(std::function<void()> &&fn);