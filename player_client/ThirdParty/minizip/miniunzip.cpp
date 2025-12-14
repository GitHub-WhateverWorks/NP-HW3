#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

#include <string>
#include <sys/stat.h>
#include <filesystem>
#include <iostream>
#include "miniunzip.hpp"

bool unzipToDir(const std::string &zipPath, const std::string &outDir) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) {
        std::cerr << "Failed to open ZIP: " << zipPath << "\n";
        return false;
    }

    std::filesystem::create_directories(outDir);

    int fileCount = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < fileCount; i++) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;

        std::string outPath = outDir + "/" + st.m_filename;

        // Directory?
        if (st.m_is_directory) {
            std::filesystem::create_directories(outPath);
            continue;
        }

        // Create parent dirs
        std::filesystem::create_directories(
            std::filesystem::path(outPath).parent_path()
        );

        if (!mz_zip_reader_extract_to_file(&zip, i, outPath.c_str(), 0)) {
            std::cerr << "Failed to extract " << st.m_filename << "\n";
        } else {
            std::cout << "Extracted: " << outPath << "\n";
        }
    }

    mz_zip_reader_end(&zip);
    return true;
}
