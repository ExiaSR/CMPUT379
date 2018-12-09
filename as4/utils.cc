/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "utils.h"
#include <sstream>
#include <fstream>
#include <string>

using namespace std;

// Split string by delimiter
vector<string> split(string s, char delimiter) {
  istringstream ss(s);
  vector<string> tokens;
  string buffer;

  while (getline(ss, buffer, delimiter)) {
    tokens.push_back(buffer);
  }

  return tokens;
}
