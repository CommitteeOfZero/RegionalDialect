#include <cstdint>
#include <map>
#include <string>

#include "skyline/utils/cpputils.hpp"
#include "skyline/logger/StdoutLogger.hpp"

#include "RegionalDialect/Hook.h"
#include "RegionalDialect/Utils.h"
#include "RegionalDialect/Config.h"

enum SigExprTokenType {
    Start,
    Plus,
    Minus,
    Deref,
    LParen,
    RParen,
    Comma,
    Number,
    Ptr,
    End
};

struct SigExprToken {
    SigExprTokenType type;
    uintptr_t value;
};

class SigExprLexer {
  private:
    const std::string &input;
    size_t pos = 0;
    SigExprToken currentToken;
    const static std::map<const char, SigExprTokenType> tokenMapping;
  public:
    friend class SigExprParser;

    SigExprLexer(const std::string &input) : input(input), currentToken({ Start, 0 }) {}

    SigExprToken getToken() {
        if (currentToken.type == Start) nextToken();
        return currentToken;
    }

    void nextToken() {
        while (pos < input.size() && std::isspace(input[pos])) pos++;
        
        decltype(tokenMapping)::const_iterator it;

        if (pos >= input.size()) currentToken = {End, 0};
        else if ((it = tokenMapping.find(input[pos])) != tokenMapping.end()) { currentToken = { it->second, 0 }; pos++; }
        else if (std::isdigit(input[pos])) {
            char *end;
            currentToken = { Number, (uintptr_t)std::strtoull(input.c_str() + pos, &end, 0) };
            pos = end - input.c_str();
        } else if (pos + 2 < input.size() && strncasecmp(input.c_str() + pos, "ptr", 3) == 0) {
            currentToken = { Ptr, 0 };
            pos += 3;
        } else {
            skyline::logger::s_Instance->LogFormat("Lexing error in '%s' at position %lu: Unexpected character: '%c'", input.c_str(), pos, input[pos]);
            std::exit(1);
        }
    }
};

class SigExprParser {
  private:
    SigExprLexer lexer;
    const uintptr_t ptr;
    const static std::map<SigExprTokenType, const std::string&> tokenType;

    uintptr_t expression(bool allowComma = true) {
        uintptr_t result = summand(false, allowComma);

        SigExprToken token = lexer.getToken();
        while (token.type == Plus || token.type == Minus) {
            lexer.nextToken();

            result += summand(false, allowComma) * (token.type == Plus ? 1 : -1);
            token = lexer.getToken();
        }
        return result;
    }

    uintptr_t summand(bool onlyDereferable, bool allowComma = true) {
        SigExprToken token = lexer.getToken();
        lexer.nextToken();

        switch (token.type) {
            case Deref: {
                return *(uint64_t*)(summand(true, allowComma));
            }
            case LParen: {
                uintptr_t result = expression();
                SigExprToken token = lexer.getToken();
                lexer.nextToken();

                if (token.type == Comma && allowComma) {
                    uintptr_t offset = expression(false);
                    result = rd::utils::retrievePointer(result, offset);
                    token = lexer.getToken();
                    lexer.nextToken();
                } 
                
                if (token.type != RParen) break;
                return result;
            }
            case Ptr: {
                return ptr;
            }
            case Number: {
                if (!onlyDereferable) return token.value;
                break;
            }
            default:
                break;
        }

        skyline::logger::s_Instance->LogFormat("Parsing error in '%s': Expected EOL, got %s.\n", lexer.input.c_str(), tokenType.find(lexer.getToken().type)->second.c_str());
        std::exit(1);
    }

  public:
    SigExprParser(const std::string& input, uintptr_t ptr) : lexer(input), ptr(ptr) {}

    uintptr_t eval() {
        uintptr_t result = expression();
        if (lexer.getToken().type != End) {
            skyline::logger::s_Instance->LogFormat("Parsing error in '%s': Expected EOL, got %s.\n", lexer.input.c_str(), tokenType.find(lexer.getToken().type)->second.c_str());
            std::exit(1);
        }
        return result;
    }
};

const std::map<const char, SigExprTokenType> SigExprLexer::tokenMapping = {
    { '+', Plus,  }, { '-', Minus  },
    { '*', Deref  }, { '(', LParen },
    { ')', RParen }, { ',', Comma  },
};

const std::map<SigExprTokenType, const std::string&> SigExprParser::tokenType = {
    { Start,  "Start"  }, { Plus,   "Plus"   },
    { Minus,  "Minus"  }, { Deref,  "Deref"  },
    { LParen, "LParen" }, { RParen, "RParen" },
    { Comma,  "Comma"  }, { Number, "Number" },
    { Ptr,    "Ptr"    }, { End,    "End"    }
};

using namespace std;

namespace rd {
namespace hook {

struct PatternByte {
    struct PatternNibble {
        unsigned char data;
        bool wildcard;
    } nibble[2];
};

static string FormatPattern(string patterntext) {
    string result;
    int len = patterntext.length();
    for (int i = 0; i < len; i++)
        if (patterntext[i] == '?' || isxdigit(patterntext[i]))
        result += toupper(patterntext[i]);
    return result;
}

static int HexChToInt(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return 0;
}

static bool TransformPattern(string patterntext, vector<PatternByte>& pattern) {
    pattern.clear();
    patterntext = FormatPattern(patterntext);
    int len = patterntext.length();
    if (!len) return false;

    if (len % 2)  // not a multiple of 2
    {
        patterntext += '?';
        len++;
    }

    PatternByte newByte;
    for (int i = 0, j = 0; i < len; i++) {
        if (patterntext[i] == '?')  // wildcard
        {
        newByte.nibble[j].wildcard = true;  // match anything
        } else                                // hex
        {
        newByte.nibble[j].wildcard = false;
        newByte.nibble[j].data = HexChToInt(patterntext[i]) & 0xF;
        }

        j++;
        if (j == 2)  // two nibbles = one byte
        {
        j = 0;
        pattern.push_back(newByte);
        }
    }
    return true;
}

static bool MatchByte(const unsigned char byte, const PatternByte& pbyte) {
    int matched = 0;

    unsigned char n1 = (byte >> 4) & 0xF;
    if (pbyte.nibble[0].wildcard)
        matched++;
    else if (pbyte.nibble[0].data == n1)
        matched++;

    unsigned char n2 = byte & 0xF;
    if (pbyte.nibble[1].wildcard)
        matched++;
    else if (pbyte.nibble[1].data == n2)
        matched++;

    return (matched == 2);
}

uintptr_t FindPattern(const unsigned char* dataStart,
                      const unsigned char* dataEnd, const char* pszPattern,
                      uintptr_t baseAddress, size_t offset, int occurrence) {
    // Build vectored pattern..
    vector<PatternByte> patterndata;
    if (!TransformPattern(pszPattern, patterndata)) return NULL;

    // The result count for multiple results..
    int resultCount = 0;
    const unsigned char* scanStart = dataStart;

    while (true) {
        // Search for the pattern..
        const unsigned char* ret = search(scanStart, dataEnd, patterndata.begin(),
                                        patterndata.end(), MatchByte);

        // Did we find a match..
        if (ret != dataEnd) {
        // If we hit the usage count, return the result..
        if (occurrence == 0 || resultCount == occurrence)
            return baseAddress + distance(dataStart, ret) + offset;

        // Increment the found count and scan again..
        resultCount++;
        scanStart = ++ret;
        } else
        break;
    }

    return NULL;
}

uintptr_t SigScanRaw(const char *category, const char* sigName) {
    skyline::logger::s_Instance->LogFormat("SigScan: looking for %s/%s...\n", category, sigName);

    rd::config::JsonWrapper sig = rd::config::config["gamedef"]["signatures"][category][sigName];
    const char* pattern = sig["pattern"].get<char*>();
    size_t offset = sig["offset"].get<size_t>();

    skyline::logger::s_Instance->Log(pattern);

    uintptr_t baseAddress = skyline::utils::g_MainTextAddr;
    uintptr_t endAddress = skyline::utils::g_MainRodataAddr;
    
    uintptr_t retval = FindPattern((unsigned char*)baseAddress,
                                    (unsigned char*)endAddress,
                                    pattern, baseAddress, offset,
                                    sig["occurrence"].get<int>());

    if (retval != 0) {
        skyline::logger::s_Instance->LogFormat(" found at 0x%08X!\n", retval);
        return retval;
    }

    skyline::logger::s_Instance->Log(" not found!\n");
    return NULL;
}

uintptr_t SigScan(const char* category, const char* sigName) {
    if (!rd::config::config["gamedef"]["signatures"][category].has(sigName)){
        skyline::logger::s_Instance->LogFormat("Signature for %s is missing!\n", sigName);
        return 0;
    }

    uintptr_t raw = SigScanRaw(category, sigName);
    rd::config::JsonWrapper sig = rd::config::config["gamedef"]["signatures"][category][sigName];
    if (!sig.has("expr")) return raw;
    if (raw == 0) return raw;
    return SigExprParser(sig["expr"].get<char*>(), raw).eval();
}


}  // namespace hook
}  // namespace rd