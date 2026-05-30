#include "sim_engine.h"
#include <behaviortree_cpp/bt_factory.h>
#include <sstream>
#include <cstdio>
#include <stdexcept>
#include <variant>

// ── Demo BT nodes ─────────────────────────────────────────────────────────────
// Each node appends one line to the log vector on the blackboard, then
// returns SUCCESS immediately (SyncActionNode = single-tick, synchronous).

static const char* k_log_key = "log";

class demo_move_to_wp : public BT::SyncActionNode
{
public:
    demo_move_to_wp(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<std::string>("wp_id"),
            BT::InputPort<double>("arrival_m")
        };
    }

    BT::NodeStatus tick() override
    {
        auto wp_id     = getInput<std::string>("wp_id").value_or("?");
        auto arrival_m = getInput<double>("arrival_m").value_or(10.0);

        auto* log = config().blackboard->get<std::vector<std::string>*>(k_log_key);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "[%d] MOVE_TO_WP → %s  (arrival: %.0f m)",
                      static_cast<int>(log->size()) + 1,
                      wp_id.c_str(), arrival_m);
        log->push_back(buf);
        return BT::NodeStatus::SUCCESS;
    }
};

class demo_search_area : public BT::SyncActionNode
{
public:
    demo_search_area(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<double>("radius_m"),
            BT::InputPort<double>("spacing_m")
        };
    }

    BT::NodeStatus tick() override
    {
        auto radius  = getInput<double>("radius_m").value_or(200.0);
        auto spacing = getInput<double>("spacing_m").value_or(40.0);

        auto* log = config().blackboard->get<std::vector<std::string>*>(k_log_key);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "[%d] SEARCH_AREA  radius=%.0f m  spacing=%.0f m",
                      static_cast<int>(log->size()) + 1, radius, spacing);
        log->push_back(buf);
        return BT::NodeStatus::SUCCESS;
    }
};

class demo_loiter : public BT::SyncActionNode
{
public:
    demo_loiter(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<double>("duration_s") };
    }

    BT::NodeStatus tick() override
    {
        auto dur = getInput<double>("duration_s").value_or(30.0);

        auto* log = config().blackboard->get<std::vector<std::string>*>(k_log_key);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "[%d] LOITER  %.0f s",
                      static_cast<int>(log->size()) + 1, dur);
        log->push_back(buf);
        return BT::NodeStatus::SUCCESS;
    }
};

class demo_rtb : public BT::SyncActionNode
{
public:
    demo_rtb(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus tick() override
    {
        auto* log = config().blackboard->get<std::vector<std::string>*>(k_log_key);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "[%d] RTB — return to base",
                      static_cast<int>(log->size()) + 1);
        log->push_back(buf);
        return BT::NodeStatus::SUCCESS;
    }
};

// ── XML builder ───────────────────────────────────────────────────────────────

static void emit_node(std::ostringstream& xml,
                      const mission_item& cmd,
                      const std::string&  indent)
{
    std::visit([&](const auto& p)
    {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, move_to_wp_params>)
        {
            xml << indent << "<move_to_wp wp_id=\"" << p.wp_id
                << "\" arrival_m=\"" << p.arrival_m << "\"/>\n";
        }
        else if constexpr (std::is_same_v<T, search_area_params>)
        {
            xml << indent << "<search_area radius_m=\"" << p.radius_m
                << "\" spacing_m=\"" << p.spacing_m << "\"/>\n";
        }
        else if constexpr (std::is_same_v<T, loiter_params>)
        {
            xml << indent << "<loiter duration_s=\"" << p.duration_s << "\"/>\n";
        }
        else if constexpr (std::is_same_v<T, rtb_params>)
        {
            xml << indent << "<rtb/>\n";
        }
        // repeat_params handled by caller
    }, cmd.params);
}

std::string sim_engine::build_xml(const mission& m) const
{
    std::ostringstream xml;
    xml << "<root BTCPP_format=\"4\">\n";
    xml << "  <BehaviorTree ID=\"mission\">\n";
    xml << "    <Sequence>\n";

    const auto& cmds = m.commands;
    for (std::size_t i = 0; i < cmds.size(); ++i)
    {
        const auto& cmd = cmds[i];

        if (cmd.type == cmd_type::repeat)
        {
            const auto& p = std::get<repeat_params>(cmd.params);
            xml << "      <Repeat num_cycles=\"" << p.count << "\">\n";
            if (i + 1 < cmds.size())
            {
                ++i;
                emit_node(xml, cmds[i], "        ");
            }
            xml << "      </Repeat>\n";
        }
        else
        {
            emit_node(xml, cmd, "      ");
        }
    }

    xml << "    </Sequence>\n";
    xml << "  </BehaviorTree>\n";
    xml << "</root>\n";
    return xml.str();
}

// ── Public API ────────────────────────────────────────────────────────────────

bool sim_engine::run(const mission& m)
{
    log_.clear();
    has_result_ = false;
    succeeded_  = false;

    if (m.commands.empty())
    {
        log_.push_back("ERROR: mission has no commands.");
        has_result_ = true;
        return false;
    }

    try
    {
        BT::BehaviorTreeFactory factory;
        factory.registerNodeType<demo_move_to_wp>("move_to_wp");
        factory.registerNodeType<demo_search_area>("search_area");
        factory.registerNodeType<demo_loiter>("loiter");
        factory.registerNodeType<demo_rtb>("rtb");

        std::string xml = build_xml(m);

        auto bb = BT::Blackboard::create();
        bb->set(k_log_key, &log_);

        BT::Tree tree = factory.createTreeFromText(xml, bb);

        // All nodes are synchronous — single tick completes the whole tree
        BT::NodeStatus status = tree.tickOnce();
        // Safety loop in case any node is accidentally RUNNING
        int guard = 0;
        while (status == BT::NodeStatus::RUNNING && guard++ < 1000)
            status = tree.tickOnce();

        succeeded_  = (status == BT::NodeStatus::SUCCESS);
        has_result_ = true;

        if (!succeeded_)
            log_.push_back("ERROR: tree returned FAILURE.");

        return succeeded_;
    }
    catch (const std::exception& e)
    {
        log_.push_back(std::string("ERROR: ") + e.what());
        has_result_ = true;
        return false;
    }
}
