#pragma once

#include <string>
#include <vector>

#include "workoutlog/catalogue.hpp"
#include "workoutlog/models.hpp"

namespace wl {

enum class IssueSeverity { error, warning };

// One problem, addressed to a JSON path so a caller can name the exact block.
struct ValidationIssue {
    IssueSeverity severity;
    std::string path;
    std::string message;

    Json to_json() const;
    bool operator==(const ValidationIssue&) const = default;
};

// Checks a session against session.schema.json and against what the log means.
// Warn, never block (ADR-006): a half-written plan is a normal state of a file,
// so only what would corrupt the archive is an error.
std::vector<ValidationIssue> validate_session(const Session& session, const Catalogue* catalogue = nullptr);

bool has_errors(const std::vector<ValidationIssue>& issues);

} // namespace wl
