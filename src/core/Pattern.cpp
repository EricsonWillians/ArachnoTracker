#include "Pattern.h"

#include <utility>

namespace arachno {

Pattern::Pattern(std::string name, int rows, int tracks)
    : name_(std::move(name)), trackCount_(tracks) {
    if (rows <= 0) {
        throw std::invalid_argument("pattern must have at least one row");
    }
    if (tracks <= 0) {
        throw std::invalid_argument("pattern must have at least one track");
    }

    rows_.reserve(static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        rows_.emplace_back(tracks);
    }
}

PatternStep& Pattern::step(int row, int track) {
    checkBounds(row, track);
    return rows_[static_cast<std::size_t>(row)].steps[static_cast<std::size_t>(track)];
}

const PatternStep& Pattern::step(int row, int track) const {
    checkBounds(row, track);
    return rows_[static_cast<std::size_t>(row)].steps[static_cast<std::size_t>(track)];
}

void Pattern::checkBounds(int row, int track) const {
    if (row < 0 || row >= rowCount()) {
        throw std::out_of_range("pattern row is out of range");
    }
    if (track < 0 || track >= trackCount_) {
        throw std::out_of_range("pattern track is out of range");
    }
}

} // namespace arachno
