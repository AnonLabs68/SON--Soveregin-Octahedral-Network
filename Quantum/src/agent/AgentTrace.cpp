#include "agent/AgentTrace.h"

#include <algorithm>
#include <utility>

namespace quantum {

void AgentTrace::record(TraceKind kind, std::string title, std::string detail) {
    entries_.push_back({kind, std::move(title), std::move(detail), nowUnix()});
    if (entries_.size() > 500) {
        entries_.erase(entries_.begin(), entries_.begin() + static_cast<long long>(entries_.size() - 500));
    }
}

void AgentTrace::clear() {
    entries_.clear();
}

const std::vector<AgentTraceEntry>& AgentTrace::entries() const {
    return entries_;
}

std::vector<AgentTraceEntry> AgentTrace::latest(int limit) const {
    const int count = std::min(limit, static_cast<int>(entries_.size()));
    std::vector<AgentTraceEntry> result;
    result.reserve(static_cast<size_t>(count));
    for (int i = static_cast<int>(entries_.size()) - count; i < static_cast<int>(entries_.size()); ++i) {
        if (i >= 0) {
            result.push_back(entries_[static_cast<size_t>(i)]);
        }
    }
    return result;
}

std::int64_t AgentTrace::nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

const char* traceKindLabel(TraceKind kind) {
    switch (kind) {
    case TraceKind::System: return "system";
    case TraceKind::Snapshot: return "snapshot";
    case TraceKind::Model: return "model";
    case TraceKind::Action: return "action";
    case TraceKind::Memory: return "memory";
    case TraceKind::Error: return "error";
    }
    return "trace";
}

} // namespace quantum

