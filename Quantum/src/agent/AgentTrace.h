#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace quantum {

enum class TraceKind {
    System,
    Snapshot,
    Model,
    Action,
    Memory,
    Error
};

struct AgentTraceEntry {
    TraceKind kind = TraceKind::System;
    std::string title;
    std::string detail;
    std::int64_t timestampUnix = 0;
};

class AgentTrace {
public:
    void record(TraceKind kind, std::string title, std::string detail = {});
    void clear();
    const std::vector<AgentTraceEntry>& entries() const;
    std::vector<AgentTraceEntry> latest(int limit = 80) const;

private:
    static std::int64_t nowUnix();

    std::vector<AgentTraceEntry> entries_;
};

const char* traceKindLabel(TraceKind kind);

} // namespace quantum

