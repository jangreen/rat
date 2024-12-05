#include <spdlog/spdlog.h>

#include <iostream>

#include "src/Rat.h"
#include "src/helper/utility.h"
#include "src/parsing/LogicVisitor.h"

int main(int argc, const char *argv[]) {
#if DEBUG
  spdlog::set_level(spdlog::level::debug);  // Set global log level to debug
#endif
  // parse arguments or ask for arguments argv[0] is path to executed program
  std::vector<std::string> programArguments;
  if (argc > 1) {
    programArguments.assign(argv + 1, argv + argc);
  } else {
    std::cout << "> " << std::flush;
    std::string input;
    std::getline(std::cin, input);
    std::stringstream inputStream(input);
    std::string argument;
    while (inputStream >> argument) {
      programArguments.push_back(argument);
    }
  }

  std::string path = programArguments[0];
  rat(path);
}
