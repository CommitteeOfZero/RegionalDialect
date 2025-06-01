#pragma once

#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "common.hpp"
#include "types.h"
#include "skyline/nn/fs.h"

namespace skyline::utils {

void init();

Result walkDirectory(std::string const&,
                     std::function<void(nn::fs::DirectoryEntry const&, std::shared_ptr<std::string>)>,
                     bool recursive = true);
Result readEntireFile(std::string const&, void**, size_t*);
Result readFile(std::string const&, s64, void*, size_t);
Result writeFile(std::string const&, s64, void*, size_t);
Result entryCount(u64*, std::string const&, nn::fs::DirectoryEntryType);
};  // namespace skyline::utils