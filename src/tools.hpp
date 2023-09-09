#ifndef DUPTREE_TOOLS_HPP
#define DUPTREE_TOOLS_HPP

#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <ios>
#include <sstream>
#include <map>

typedef long long Int;
inline std::string itos(Int decimal) {
    std::string ret;
    for (int i = 0; i < 10; ++i) {
        ret.append("0");
    }
    ret.append(std::to_string(decimal));
    return ret;
    char s[20];
    s[19] = 0;
    s[18] = '0' + (decimal & 1);
    Int i = 17;
    Int l = 2;
    for (; l <= decimal; --i, l <<= 1) {
        if ((decimal & l) > 0) {
            s[i] = '1';
        }
        else {
            s[i] = '0';
        }
    }

    std::string binary(s + i + 1);
    return binary;
}

std::string calculateSHA256Hash(const std::string& data);
std::string random_string(Int len = 64);

Int up_to(Int x, Int v);
Int up_to_max(Int x, Int v);
void random_sample(std::vector<Int> &sample, Int number, Int max_value);

std::string hex_to_binary(const std::string &hex);
std::string int_to_binary(Int decimal, Int numBits);
std::string int_to_hex(Int decimal, Int numBits);
std::string int_to_hex(Int decimal);

extern std::map<char, int> hti;
extern std::string null64;
bool is_prefix(const std::string &str, const std::string &pre);
std::string common_prefix(const std::string &s1, const std::string &s2);

#endif //DUPTREE_TOOLS_HPP
