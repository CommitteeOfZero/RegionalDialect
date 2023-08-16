#include <string>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <sstream>

#include "skyline/utils/cpputils.hpp"
#include "skyline/logger/TcpLogger.hpp"
#include "skyline/logger/StdoutLogger.hpp"

#include "json.hpp"
using json = nlohmann::json;

#include "SigScan.h"
#include "SigExpr.h"
#include "Config.h"

using namespace std;

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

uintptr_t sigScanRaw(const char *category, const char* sigName) {

  std::stringstream logstr;
  logstr << "SigScan: looking for " << category << "/" << sigName << "... "
         << std::endl;

  json sig = config["gamedef"]["signatures"][category][sigName];
  std::string sPattern = sig["pattern"].get<std::string>();
  const char* pattern = sPattern.c_str();
  size_t offset = sig["offset"].get<size_t>();

  logstr << sPattern << std::endl;

  uintptr_t baseAddress = skyline::utils::g_MainTextAddr;
  uintptr_t endAddress = skyline::utils::g_MainRodataAddr;
  uintptr_t retval = FindPattern((unsigned char *)baseAddress,
                                 (unsigned char*)endAddress,
                                 pattern, baseAddress,offset,
                                 sig["occurrence"].get<int>());

  if (retval != NULL) {
      // logstr << " found at 0x" << std::hex << retval;
      // skyline::logger::s_Instance->Log(logstr.str().c_str());
      return retval;
  }

  // logstr << " not found!";
  // skyline::logger::s_Instance->LogFormat("%s", logstr.str().c_str());
  return NULL;
}


uintptr_t sigScan(const char* category, const char* sigName) {
  if (config["gamedef"]["signatures"][category].count(sigName) == 0){
    return NULL;
  }

  uintptr_t raw = sigScanRaw(category, sigName);
  json sig = config["gamedef"]["signatures"][category][sigName];
  if (sig.count("expr") == 0) return raw;
  if (raw == 0) return raw;
  return SigExpr(sig["expr"].get<std::string>(), raw).evaluate();
}