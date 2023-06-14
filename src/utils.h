#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

static inline bool file_exists(const std::string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }   
}

static std::vector<uint8_t> load_test_file(const std::string& filename) {
  // Open the file in binary mode.
  std::ifstream file(filename, std::ios::binary);

  // Read the contents of the file into a vector.
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  return data;
}
