#pragma once

#include "core/data_model.h"
#include <string>

namespace mission_io
{
    // Serialise the full mission to a JSON file.
    // Returns true on success.
    bool save(const mission& m, const std::string& path);

    // Deserialise a mission from a JSON file.
    // Returns true on success; m is unchanged on failure.
    bool load(mission& m, const std::string& path);
}
