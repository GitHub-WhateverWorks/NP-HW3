#include "base64.hpp"
#include <stdexcept>

static const std::string b64chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::vector<uint8_t> decodeBase64(const std::string &input) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[b64chars[i]] = i;

    int val = 0;
    int valb = -8;

    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;

        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return out;
}

std::string encodeBase64(const std::vector<uint8_t> &data) {
    std::string out;
    int val = 0;
    int valb = -6;

    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;

        while (valb >= 0) {
            out.push_back(b64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        out.push_back(b64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (out.size() % 4) out.push_back('=');

    return out;
}
