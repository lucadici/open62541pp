#pragma once

#include "open62541pp/condition.hpp"
#include <iostream>
#include <ostream>

namespace opcua {

/**
 * Convenience wrapper for a simple on/off (ActiveState) alarm condition.
 * Creates an OffNormalAlarmType instance and exposes helpers to toggle active state
 * while updating common fields (Message, Retain, Time) and triggering the event.
 */
class OnOffCondition : public Condition {
public:
    OnOffCondition(Server& server,
                   const NodeId& source,
                   std::string_view name = "OnOffCondition",
                   const NodeId& parentReferenceType = ReferenceTypeId::HasComponent,
                   uint16_t initialSeverity = 400)
        : Condition(server,
                    ObjectTypeId::OffNormalAlarmType,
                    QualifiedName{0, name},
                    source,
                    parentReferenceType),
          source_{source} {
        // Enable and set default fields
        setVariableField({0, "EnabledState"}, {0, "Id"}, Variant{true});
        setField({0, "Severity"}, Variant{initialSeverity});
        setField({0, "Message"}, Variant{LocalizedText{"", "Alarm inactive"}});
        setField({0, "Retain"}, Variant{false});
        // Optional fields not added by default
    }

    // Toggle active state; updates Message/Time/Retain and triggers event.
    void setActive(const NodeId& source, bool active,
                   std::string_view message = {}) {
        std::cout << "[A&C] OnOffCondition::setActive -> " << (active ? "true" : "false") << std::endl;
        if (!message.empty()) {
            setField({0, "Message"}, Variant{LocalizedText{"", std::string{message}}});
        } else {
            setField({0, "Message"}, Variant{LocalizedText{"", active ? "Alarm active" : "Alarm inactive"}});
        }
        setField({0, "Time"}, Variant{DateTime::now()});
        setField({0, "Retain"}, Variant{active});
        setVariableField({0, "ActiveState"}, {0, "Id"}, Variant{active});
        if (!active) {
            // Reset acknowledgment and confirmation when alarm becomes inactive
            setVariableField({0, "AckedState"}, {0, "Id"}, Variant{false});
            setVariableField({0, "ConfirmedState"}, {0, "Id"}, Variant{false});
        }
        trigger(source);
    }

    using Condition::onAboutToBeAcked;
    using Condition::onAboutToBeConfirmed;
    using Condition::onEnabled;
    using Condition::onActive;

    // Convenience overloads: bind to the stored source node
#ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
    OnOffCondition& onEnabled(std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb, bool removeBranch=false) {
        Condition::onEnabled(source_, std::move(cb), removeBranch);
        return *this;
    }
    OnOffCondition& onAboutToBeAcked(std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb, bool removeBranch=false) {
        Condition::onAboutToBeAcked(source_, std::move(cb), removeBranch);
        return *this;
    }
    OnOffCondition& onAboutToBeConfirmed(std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb, bool removeBranch=false) {
        Condition::onAboutToBeConfirmed(source_, std::move(cb), removeBranch);
        return *this;
    }
    OnOffCondition& onActive(std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb, bool removeBranch=false) {
        Condition::onActive(source_, std::move(cb), removeBranch);
        return *this;
    }

    // Note: no bool-only overloads here to avoid ambiguity with lambdas
#endif

private:
    NodeId source_;
};

}  // namespace opcua
