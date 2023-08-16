#include <cctype>
#include <sstream>
#include <string.h>


#include "skyline/logger/TcpLogger.hpp"
#include "skyline/logger/StdoutLogger.hpp"

#include "SigExpr.h"

SigExprToken_t SigExprLexer::getToken() {
  if (currentToken.type == Start) consumeToken();
  return currentToken;
}

void SigExprLexer::consumeToken() {
  while (pos < input.size() && std::isspace(input.at(pos))) pos++;
  if (pos < input.size()) {
    if (input.at(pos) == '+') {
      currentToken = {Plus, 0};
      pos++;
    } else if (input.at(pos) == '-') {
      currentToken = {Minus, 0};
      pos++;
    } else if (input.at(pos) == '*') {
      currentToken = {Deref, 0};
      pos++;
    } else if (input.at(pos) == '(') {
      currentToken = {LParen, 0};
      pos++;
    } else if (input.at(pos) == ')') {
      currentToken = {RParen, 0};
      pos++;
    } else if (std::isdigit(input.at(pos))) {
      char* end;
      currentToken = {Number, (uintptr_t)std::strtoull(input.c_str() + pos, &end, 0)};
      pos = end - input.c_str();
    } else if (pos + 2 < input.size() &&
               strncasecmp(input.c_str() + pos, "ptr", 3) == 0) {
      currentToken = {Ptr, 0};
      pos += 3;
    } else {
      // std::stringstream ss;
      // ss << "Lex error in '" << input << "at position " << pos
      //    << ": unexpected character '" << input.at(pos) << "'";
      // skyline::logger::s_Instance->Log(ss.str());
      // exit(1);
    }
  } else {
    currentToken = {EOL, 0};
  }
}

uintptr_t SigExpr::evaluate() {
  uintptr_t result = expression();
  SigExprToken_t expectedEol = lexer.getToken();
  if (expectedEol.type != EOL) {
    // std::stringstream ss;
    // ss << "Parse error in '" << input << ": Expected EOL, got "
    //    << SigExprTokenTypeString.at(expectedEol.type);
    // skyline::logger::s_Instance->Log(ss.str());
    // exit(1);
  }
  return result;
}

uintptr_t SigExpr::expression() {
  uintptr_t result = summand(false);

  SigExprToken_t token = lexer.getToken();
  while (token.type == Plus || token.type == Minus) {
    lexer.consumeToken();
    if (token.type == Plus)
      result += summand(false);
    else
      result -= summand(false);
    token = lexer.getToken();
  }

  return result;
}

uintptr_t SigExpr::summand(bool onlyDereferable) {
  SigExprToken_t token = lexer.getToken();
  lexer.consumeToken();

  switch (token.type) {
    case Deref: {
      return *(uint32_t*)(summand(true));
      break;
    }
    case LParen: {
      uintptr_t result = expression();
      SigExprToken_t expectedRparen = lexer.getToken();
      lexer.consumeToken();
      if (expectedRparen.type != RParen) {
        // std::stringstream ss;
        // ss << "Parse error in '" << input << ": Expected RParen, got "
        //    << SigExprTokenTypeString.at(expectedRparen.type);
        // skyline::logger::s_Instance->Log(ss.str());
        // exit(1);
      }
      return result;
      break;
    }
    case Ptr: {
      return ptr;
      break;
    }
    case Number: {
      if (!onlyDereferable) return token.value;
      break;
    }
  }

  // if we're still here
  // std::stringstream ss;
  // ss << "Parse error in '" << input << ": Unexpected token "
  //    << SigExprTokenTypeString.at(token.type);
  // skyline::logger::s_Instance->Log(ss.str());
  // exit(1);
}