#include <smark.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

const std::unordered_map<std::string, smark::LanguageCode> languages{
    {"en", smark::LanguageCode::EN},
    {"de", smark::LanguageCode::DE},
    {"es", smark::LanguageCode::ES},
    {"fr", smark::LanguageCode::FR},
};

int main(int argc, char** argv) {
  cxxopts::Options options(argv[0], "A program to welcome the world!");

  std::string language;
  std::string name;

  // clang-format off
  options.add_options()
    ("h,help", "Show help")
    ("n,name", "Name to greet", cxxopts::value(name)->default_value("World"))
    ("l,lang", "Language code to use", cxxopts::value(language)->default_value("en"))
  ;
  // clang-format on

  auto result = options.parse(argc, argv);

  if (result["help"].as<bool>()) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  auto langIt = languages.find(language);
  if (langIt == languages.end()) {
    std::cout << "unknown language code: " << language << std::endl;
    return 1;
  }

  smark::Smark smark(name);
  std::cout << smark.greet(langIt->second) << std::endl;

  return 0;
}
