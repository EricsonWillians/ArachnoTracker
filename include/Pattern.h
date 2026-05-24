#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "PatternRow.h"

namespace arachno {

class Pattern {
public:
    Pattern(std::string name = "Pattern", int rows = 64, int tracks = 8);

    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    int rowCount() const { return static_cast<int>(rows_.size()); }
    int trackCount() const { return trackCount_; }
    void resizeRows(int rows);
    void resizeTracks(int tracks);

    PatternStep& step(int row, int track);
    const PatternStep& step(int row, int track) const;

    std::vector<PatternRow>& rows() { return rows_; }
    const std::vector<PatternRow>& rows() const { return rows_; }

private:
    void checkBounds(int row, int track) const;

    std::string name_;
    int trackCount_ = 0;
    std::vector<PatternRow> rows_;
};

} // namespace arachno
