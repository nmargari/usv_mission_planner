#pragma once

#include "core/data_model.h"
#include <vector>
#include <string>

// Demo-mode execution engine.
// Builds a BT::Tree from mission.commands, runs it once synchronously,
// and populates a text log showing each node that fired in order.
class sim_engine
{
public:
    // Runs the mission tree to completion. Returns true if the tree succeeded.
    bool run(const mission& m);

    const std::vector<std::string>& log()        const { return log_; }
    bool                            has_result()  const { return has_result_; }
    bool                            succeeded()   const { return succeeded_; }

private:
    std::string build_xml(const mission& m) const;

    std::vector<std::string> log_;
    bool                     has_result_ = false;
    bool                     succeeded_  = false;
};
