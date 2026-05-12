#pragma once
#include <string>
#include <fstream>
#include <mutex>

namespace quantum {

inline void startupLog(const std::string& line) {
    static std::mutex mtx;
    std::scoped_lock lock(mtx);
    static std::ofstream out("quantum_diagnostics.log", std::ios::app);
    out << line << "\n";
    out.flush();
}

} // namespace quantum
