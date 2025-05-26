#ifndef __SIGEXPR_H__
#define __SIGEXPR_H__

#include <cstdint>
#include <map>
#include <string>

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
    SigExprLexer(const std::string &input);
    SigExprToken getToken();
    void nextToken();
};


class SigExprParser {
  private:
    SigExprLexer lexer;
    const uintptr_t ptr;

    uintptr_t expression(bool allowComma = true);
    uintptr_t summand(bool onlyDereferable, bool allowComma = true);
    const static std::map<SigExprTokenType, const std::string&> tokenType;
  public:
    SigExprParser(const std::string& input, uintptr_t ptr);
    uintptr_t eval();
};

#endif  // !__SIGEXPR_H__
