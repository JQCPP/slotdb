#include "simplified/shell.hpp"
#include "simplified/simpledb.hpp"
#include "command/command.hpp"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

bool gQuit = false;

int main(int argc, const char *argv[]) {
  simplified::Shell shell;
  shell.start();
  return 0;
}