#pragma once
#include <string>

// Extracts a .zip file (zipPath) into outDir
// Returns true on success
bool unzipToDir(const std::string &zipPath, const std::string &outDir);
