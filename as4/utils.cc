/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved - Do Not Redistribute
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
