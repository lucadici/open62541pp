#include <iostream>

#include <open62541pp/open62541pp.hpp>

using namespace opcua;

namespace {

class ActivateConditionCallback : public ValueCallbackBase {
public:
    ActivateConditionCallback(OnOffCondition& cond, const NodeId& sourceId)
        : cond_{cond}, sourceId_{sourceId} {}

    void onRead(Session&, const NodeId&, const NumericRange*, const DataValue&) override {}

    void onWrite(Session&, const NodeId&, const NumericRange*, const DataValue& value) override {
        try {
            const bool active = value.value().to<bool>();
            cond_.setActive(sourceId_, active);
        } catch (const std::exception& ex) {
            std::cerr << "Condition activation failed: " << ex.what() << std::endl;
        }
    }

private:
    OnOffCondition& cond_;
    NodeId sourceId_;
};

}  // namespace

int main() {
#ifndef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
    std::cerr << "This example requires UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS" << std::endl;
    return 1;
#else
    // Basic server config
    ServerConfig config;
    config.setApplicationName("open62541pp A&C example");
    config.setApplicationUri("urn:open62541pp.server.alarmsconditions");

    Server server{std::move(config)};

    // Create a condition source object under Objects and mark it as event notifier
    Node objects{server, ObjectId::ObjectsFolder};
    Node source = objects.addObject(NodeId{}, "ConditionSourceObject");
    source.writeEventNotifier(EventNotifier::SubscribeToEvents);

    // Link source as notifier to Server object (HasNotifier)
    Node serverNode{server, ObjectId::Server};
    serverNode.addReference(source.id(), ReferenceTypeId::HasNotifier);

    // Create a simple on/off condition exposed under the source
    OnOffCondition cond{server, source.id(), "OnOffCondition", ReferenceTypeId::HasComponent, 400};
    // Optional: set a readable source name for clients
    cond.setField({0, "SourceName"}, Variant{std::string{"ConditionSourceObject"}});

    // Resolve condition display name for logs
    const std::string condName = std::string{opcua::Node(server, cond.id()).readDisplayName().text()};

    // Register ergonomic two-state callbacks and log transitions with condition name
    cond
        .onEnabled([&condName](const NodeId&, bool) {
            std::cout << "[A&C] Enabled=true | condition=" << condName << std::endl << std::flush;
            return UA_STATUSCODE_GOOD;
        })
        .onAcked([&condName](const NodeId&, bool) {
            std::cout << "[A&C] Acked=true | condition=" << condName << std::endl << std::flush;
            return UA_STATUSCODE_GOOD;
        })
        .onConfirmed([&condName](const NodeId&, bool) {
            std::cout << "[A&C] Confirmed=true | condition=" << condName << std::endl << std::flush;
            return UA_STATUSCODE_GOOD;
        })
        .onActive([&condName](const NodeId&, bool) {
            // Note: this will not be fired when alarms is deactivated, because
            // this is something managed by server, not by client
            std::cout << "[A&C] Active=true | condition=" << condName << std::endl << std::flush;
            return UA_STATUSCODE_GOOD;
        });

    // Variable to activate/deactivate Condition 1 (writes ActiveState/Id)
    VariableAttributes activateAttr;
    activateAttr.setDisplayName({"en-US", "AlarmActive"});
    activateAttr.setAccessLevel(AccessLevel::CurrentRead | AccessLevel::CurrentWrite);
    activateAttr.setUserAccessLevel(AccessLevel::CurrentRead | AccessLevel::CurrentWrite);
    activateAttr.setDataType<bool>();
    activateAttr.setValue(Variant{false});
    Node activateVar = objects.addVariable(NodeId{}, "AlarmActive", activateAttr);
    setVariableNodeValueCallback(
        server,
        activateVar.id(),
        std::make_unique<ActivateConditionCallback>(cond, source.id())
    );

    std::cout << "A&C example running. Write true/false to AlarmActive." << std::endl;
    server.run();
    return 0;
#endif
}
