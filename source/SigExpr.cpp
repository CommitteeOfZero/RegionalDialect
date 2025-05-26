#include <cctype>
#include <sstream>
#include <string.h>

#include "skyline/logger/StdoutLogger.hpp"

#include "MemUtils.h"
#include "SigExpr.h"

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
    
SigExprLexer::SigExprLexer(const std::string &input) : input(input), currentToken({ Start, 0 }) {}

SigExprToken SigExprLexer::getToken() {
    if (currentToken.type == Start) nextToken();
    return currentToken;
}

void SigExprLexer::nextToken() {
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
        skyline::logger::s_Instance->LogFormat("Lexing error in '%s' at position %lu: Unexpected character: '%c'", input, pos, input[pos]);
        std::exit(1);
    }
}

SigExprParser::SigExprParser(const std::string &input, uintptr_t ptr) : lexer(input), ptr(ptr) {}

uintptr_t SigExprParser::eval() {
    uintptr_t result = expression();
    if (lexer.getToken().type != End) {
        skyline::logger::s_Instance->LogFormat("Parsing error in '%s': Expected EOL, got %s.\n", lexer.input, tokenType.find(lexer.getToken().type)->second.c_str());
        std::exit(1);
    }
    return result;
}

uintptr_t SigExprParser::expression(bool allowComma) {
    uintptr_t result = summand(false, allowComma);

    SigExprToken token = lexer.getToken();
    while (token.type == Plus || token.type == Minus) {
        lexer.nextToken();

        result += summand(false, allowComma) * (token.type == Plus ? 1 : -1);
        token = lexer.getToken();
    }
    return result;
}

uintptr_t SigExprParser::summand(bool onlyDereferable, bool allowComma) {
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
                result = retrievePointer(result, offset);
                token = lexer.getToken();
                lexer.nextToken();
                break;
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

    skyline::logger::s_Instance->LogFormat("Parsing error in '%s': Expected EOL, got %s.\n", lexer.input, tokenType.find(lexer.getToken().type)->second.c_str());
    std::exit(1);
}