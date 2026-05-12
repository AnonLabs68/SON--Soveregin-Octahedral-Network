#pragma once

#include <string>

namespace quantum {

std::string buildAxTreeSnapshotScript();
std::string unwrapWebViewJsonString(const std::string& scriptResult);

} // namespace quantum

