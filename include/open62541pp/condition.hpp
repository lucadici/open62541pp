#pragma once

#include <string_view>

#include "open62541pp/config.hpp"
#include "open62541pp/session.hpp"
#include "open62541pp/types.hpp"
#include "open62541pp/ua/nodeids.hpp"
#include "open62541pp/detail/open62541/server.h"  // for UA_TwoStateVariable* when enabled

namespace opcua {

class Server;

/**
 * Alarms & Conditions wrapper.
 *
 * Provides a minimal C++ interface over open62541 A&C APIs to create a
 * condition instance, set fields, and trigger condition events.
 *
 * Requires UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS in the underlying C lib.
 */
class Condition {
public:
    struct UseExisting final {};

    /// Create a condition instance in the server.
    /// @param conditionType Type of the Condition (e.g. `OffNormalAlarmType`)
    /// @param browseName Browse name of the instance
    /// @param conditionSource Source object for the condition (typically `Server` or a custom object)
    /// @param parentReferenceType Reference type to expose the condition below the source (e.g. `HasComponent`).
    ///        Pass a null NodeId to not expose the condition in the address space.
    /// @param requestedNodeId Explicit NodeId for the condition or null to let the server assign one.
    Condition(Server& connection,
              const NodeId& conditionType,
              const QualifiedName& browseName,
              const NodeId& conditionSource,
              const NodeId& parentReferenceType = NodeId{},
              const NodeId& requestedNodeId = NodeId{});

    /// Wrap an existing condition instance without taking ownership.
    Condition(Server& connection, const NodeId& existingConditionId, UseExisting) noexcept;

    virtual ~Condition();

    Condition(const Condition&) = default;
    Condition(Condition&&) noexcept = default;

    Condition& operator=(const Condition&) = delete;
    Condition& operator=(Condition&&) noexcept = delete;

    Server& connection() noexcept { return *connection_; }
    const Server& connection() const noexcept { return *connection_; }

    const NodeId& id() const noexcept { return id_; }

    /// Set a field on the condition (e.g. `Message`, `Severity`, `Time`).
    Condition& setField(const QualifiedName& field, const Variant& value);

    /// Set a nested property of a Variable field (e.g. `ActiveState/Id`).
    Condition& setVariableField(const QualifiedName& variable,
                                const QualifiedName& property,
                                const Variant& value);

    /// Trigger a condition event. Call after updating fields.
    ByteString trigger(const NodeId& conditionSource);

    /// Register a two-state variable transition callback (Enabled/Acked/Confirmed/Active enters true).
    /// The callback is invoked just before the event is triggered by the server.
#ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
    void setTwoStateCallback(const NodeId& conditionSource,
                             bool removeBranch,
                             UA_TwoStateVariableChangeCallback callback,
                             UA_TwoStateVariableCallbackType type);

    // Fluent registration that calls virtual hooks
    Condition& onEnabled(const NodeId& source,
                         std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                         bool removeBranch = false);
    Condition& onAboutToBeAcked(const NodeId& source,
                       std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                       bool removeBranch = false);
    Condition& onAboutToBeConfirmed(const NodeId& source,
                           std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                           bool removeBranch = false);
    Condition& onActive(const NodeId& source,
                        std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                        bool removeBranch = false);

    // (No message overloads; callbacks receive only source/removeBranch)
#endif

    /// Release ownership and return the underlying NodeId.
    [[nodiscard]] NodeId release() noexcept;

private:
    Server* connection_{};
    NodeId id_{};
    bool owns_ = true;

protected:
#ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
    // Virtual hooks (default no-op). Override in subclasses for ergonomic handling.
    virtual void onEnabledHook(const NodeId& /*source*/, bool /*removeBranch*/) {}
    virtual void onAboutToBeAckedHook(const NodeId& /*source*/, bool /*removeBranch*/) {}
    virtual void onAboutToBeConfirmedHook(const NodeId& /*source*/, bool /*removeBranch*/) {}
    virtual void onActiveHook(const NodeId& /*source*/, bool /*removeBranch*/) {}
#endif
};

bool operator==(const Condition& lhs, const Condition& rhs) noexcept;
bool operator!=(const Condition& lhs, const Condition& rhs) noexcept;

}  // namespace opcua
