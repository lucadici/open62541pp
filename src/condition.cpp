#include "open62541pp/condition.hpp"

#include "open62541pp/config.hpp"  // UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
#include "open62541pp/detail/open62541/server.h"
#include "open62541pp/exception.hpp"
#include "open62541pp/server.hpp"
#include "open62541pp/detail/server_context.hpp"
#include "open62541/plugin/log_stdout.h"
#include <iostream>

namespace opcua {

#ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS

Condition::Condition(Server& connection,
                     const NodeId& conditionType,
                     const QualifiedName& browseName,
                     const NodeId& conditionSource,
                     const NodeId& parentReferenceType,
                     const NodeId& requestedNodeId)
    : connection_{&connection} {
    throwIfBad(UA_Server_createCondition(connection.handle(),
                                         requestedNodeId,
                                         conditionType,
                                         browseName,
                                         conditionSource,
                                         parentReferenceType,
                                         id_.handle()));
}

Condition::~Condition() {
    if (owns_ && !id().isNull()) {
        UA_Server_deleteNode(connection().handle(), id(), true /* deleteReferences */);
    }
}

Condition::Condition(Server& connection, const NodeId& existingConditionId, UseExisting) noexcept
    : connection_{&connection}, id_{existingConditionId}, owns_{false} {}

Condition& Condition::setField(const QualifiedName& field, const Variant& value) {
    const auto status =
        UA_Server_setConditionField(connection().handle(), id(), value.handle(), field);
    throwIfBad(status);
    return *this;
}

Condition& Condition::setVariableField(const QualifiedName& variable,
                                       const QualifiedName& property,
                                       const Variant& value) {
    const auto status = UA_Server_setConditionVariableFieldProperty(
        connection().handle(), id(), value.handle(), variable, property);
    throwIfBad(status);
    return *this;
}

ByteString Condition::trigger(const NodeId& conditionSource) {
    // In A&C, triggering uses UA_Server_triggerConditionEvent
    ByteString eventId;
    const auto status = UA_Server_triggerConditionEvent(
        connection().handle(), id(), conditionSource, eventId.handle());
    throwIfBad(status);
    return eventId;
}

NodeId Condition::release() noexcept {
    return std::exchange(id_, {});
}

using opcua::detail::ServerContext;

namespace {

// Retrieve stored callbacks for a condition
static ServerContext::ConditionTwoStateCallbacks*
getCallbacks(opcua::Server& server, const opcua::NodeId& condition) {
    auto& ctx = opcua::detail::getContext(server);
    return const_cast<ServerContext::ConditionTwoStateCallbacks*>(ctx.conditionCallbacks.find(condition));
}

static UA_StatusCode twoStateThunkEnabled(UA_Server* s, const UA_NodeId* condition, const UA_NodeId* sessionId) {
    auto* srv = opcua::asWrapper(s);
    auto& condId = *opcua::asWrapper<opcua::NodeId>(condition);
    UA_LocalizedText dn{};
    (void)UA_Server_readDisplayName(s, *condition, &dn);
    UA_LOG_INFO(&UA_Server_getConfig(s)->logger, UA_LOGCATEGORY_USERLAND,
                "[A&C] EnabledState=true | condition=%.*s",
                (int)dn.text.length, (const char*)dn.text.data);
    UA_LocalizedText_clear(&dn);
    if (auto* cbs = getCallbacks(*srv, condId)) {
        if (cbs->enteringEnabled && cbs->enteringEnabled->cb) {
            opcua::NodeId sid = (sessionId != nullptr) ? *opcua::asWrapper<opcua::NodeId>(sessionId)
                                                       : opcua::NodeId{};
            opcua::Session sess(*srv, sid, nullptr);
            return cbs->enteringEnabled->cb(sess, cbs->enteringEnabled->source, cbs->enteringEnabled->removeBranch);
        }
    }
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode twoStateThunkAcked(UA_Server* s, const UA_NodeId* condition, const UA_NodeId* sessionId) {
    auto* srv = opcua::asWrapper(s);
    auto& condId = *opcua::asWrapper<opcua::NodeId>(condition);
    UA_LocalizedText dn2{};
    (void)UA_Server_readDisplayName(s, *condition, &dn2);
    UA_LOG_INFO(&UA_Server_getConfig(s)->logger, UA_LOGCATEGORY_USERLAND,
                "[A&C] AckedState=true | condition=%.*s",
                (int)dn2.text.length, (const char*)dn2.text.data);
    UA_LocalizedText_clear(&dn2);
    if (auto* cbs = getCallbacks(*srv, condId)) {
        if (cbs->enteringAcked && cbs->enteringAcked->cb) {
            opcua::NodeId sid = (sessionId != nullptr) ? *opcua::asWrapper<opcua::NodeId>(sessionId)
                                                       : opcua::NodeId{};
            opcua::Session sess(*srv, sid, nullptr);
            return cbs->enteringAcked->cb(sess, cbs->enteringAcked->source, cbs->enteringAcked->removeBranch);
        }
    }
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode twoStateThunkConfirmed(UA_Server* s, const UA_NodeId* condition, const UA_NodeId* sessionId) {
    auto* srv = opcua::asWrapper(s);
    auto& condId = *opcua::asWrapper<opcua::NodeId>(condition);
    UA_LocalizedText dn3{};
    (void)UA_Server_readDisplayName(s, *condition, &dn3);
    UA_LOG_INFO(&UA_Server_getConfig(s)->logger, UA_LOGCATEGORY_USERLAND,
                "[A&C] ConfirmedState=true | condition=%.*s",
                (int)dn3.text.length, (const char*)dn3.text.data);
    UA_LocalizedText_clear(&dn3);
    if (auto* cbs = getCallbacks(*srv, condId)) {
        if (cbs->enteringConfirmed && cbs->enteringConfirmed->cb) {
            opcua::NodeId sid = (sessionId != nullptr) ? *opcua::asWrapper<opcua::NodeId>(sessionId)
                                                       : opcua::NodeId{};
            opcua::Session sess(*srv, sid, nullptr);
            return cbs->enteringConfirmed->cb(sess, cbs->enteringConfirmed->source, cbs->enteringConfirmed->removeBranch);
        }
    }
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode twoStateThunkActive(UA_Server* s, const UA_NodeId* condition, const UA_NodeId* sessionId) {
    auto* srv = opcua::asWrapper(s);
    auto& condId = *opcua::asWrapper<opcua::NodeId>(condition);
    UA_LocalizedText dn4{};
    (void)UA_Server_readDisplayName(s, *condition, &dn4);
    UA_LOG_INFO(&UA_Server_getConfig(s)->logger, UA_LOGCATEGORY_USERLAND,
                "[A&C] ActiveState=true | condition=%.*s",
                (int)dn4.text.length, (const char*)dn4.text.data);
    UA_LocalizedText_clear(&dn4);
    if (auto* cbs = getCallbacks(*srv, condId)) {
        if (cbs->enteringActive && cbs->enteringActive->cb) {
            opcua::NodeId sid = (sessionId != nullptr) ? *opcua::asWrapper<opcua::NodeId>(sessionId)
                                                       : opcua::NodeId{};
            opcua::Session sess(*srv, sid, nullptr);
            return cbs->enteringActive->cb(sess, cbs->enteringActive->source, cbs->enteringActive->removeBranch);
        }
    }
    return UA_STATUSCODE_GOOD;
}

} // namespace

void Condition::setTwoStateCallback(const NodeId& conditionSource,
                                    bool removeBranch,
                                    UA_TwoStateVariableChangeCallback callback,
                                    UA_TwoStateVariableCallbackType type) {
    const auto status = UA_Server_setConditionTwoStateVariableCallback(
        connection().handle(), id(), conditionSource, static_cast<UA_Boolean>(removeBranch), callback, type);
    throwIfBad(status);
}

Condition& Condition::onEnabled(const NodeId& source,
                                std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                                bool removeBranch) {
    auto& ctx = opcua::detail::getContext(*connection_);
    auto* cbs = ctx.conditionCallbacks[id_];
    auto& slot = cbs->enteringEnabled.emplace();
    slot.source = source;
    slot.removeBranch = removeBranch;
    slot.cb = std::move(cb);
    setTwoStateCallback(source, removeBranch, twoStateThunkEnabled, UA_ENTERING_ENABLEDSTATE);
    return *this;
}

// removed onEnabled overload with message

Condition& Condition::onAboutToBeAcked(const NodeId& source,
                              std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                              bool removeBranch) {
    auto& ctx = opcua::detail::getContext(*connection_);
    auto* cbs = ctx.conditionCallbacks[id_];
    auto& slot = cbs->enteringAcked.emplace();
    slot.source = source;
    slot.removeBranch = removeBranch;
    slot.cb = std::move(cb);
    setTwoStateCallback(source, removeBranch, twoStateThunkAcked, UA_ENTERING_ACKEDSTATE);
    return *this;
}

// removed onAboutToBeAcked overload with message

Condition& Condition::onAboutToBeConfirmed(const NodeId& source,
                                  std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                                  bool removeBranch) {
    auto& ctx = opcua::detail::getContext(*connection_);
    auto* cbs = ctx.conditionCallbacks[id_];
    auto& slot = cbs->enteringConfirmed.emplace();
    slot.source = source;
    slot.removeBranch = removeBranch;
    slot.cb = std::move(cb);
    setTwoStateCallback(source, removeBranch, twoStateThunkConfirmed, UA_ENTERING_CONFIRMEDSTATE);
    return *this;
}

// removed onAboutToBeConfirmed overload with message

Condition& Condition::onActive(const NodeId& source,
                               std::function<UA_StatusCode(Session&, const NodeId&, bool)> cb,
                               bool removeBranch) {
    auto& ctx = opcua::detail::getContext(*connection_);
    auto* cbs = ctx.conditionCallbacks[id_];
    auto& slot = cbs->enteringActive.emplace();
    slot.source = source;
    slot.removeBranch = removeBranch;
    slot.cb = std::move(cb);
    setTwoStateCallback(source, removeBranch, twoStateThunkActive, UA_ENTERING_ACTIVESTATE);
    return *this;
}

// removed onActive overload with message

#endif

bool operator==(const Condition& lhs, const Condition& rhs) noexcept {
    return (lhs.connection() == rhs.connection()) && (lhs.id() == rhs.id());
}

bool operator!=(const Condition& lhs, const Condition& rhs) noexcept {
    return !(lhs == rhs);
}

}  // namespace opcua
