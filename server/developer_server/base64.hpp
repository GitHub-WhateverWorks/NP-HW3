#pragma once
#include <string>
#include <vector>
std::vector<uint8_t> decodeBase64(const std::string &b64);
std::string encodeBase64(const std::vector<uint8_t> &data);
