// Demonstrates handling all OPC UA user identity token types in activateSession:
// - AnonymousIdentityToken
// - UserNameIdentityToken
// - X509IdentityToken
// - IssuedIdentityToken (e.g., JWT/SAML)
//
// It decodes the token variant, logs basic details, and stores normalized
// session attributes to drive authorization decisions in later callbacks.

#include <iostream>

#include <open62541pp/node.hpp>
#include <open62541pp/plugin/accesscontrol_default.hpp>
#include <open62541pp/server.hpp>
#include <open62541pp/ua/types.hpp>

using namespace opcua;

class AccessControlAllTokens : public AccessControlDefault {
public:
    using AccessControlDefault::AccessControlDefault;  // inherit constructors

    StatusCode activateSession(
        Session& session,
        const EndpointDescription& endpointDescription,
        const ByteString& secureChannelRemoteCertificate,
        const ExtensionObject& userIdentityToken
    ) override {
        // Default values
        std::string identityType = "Unknown";
        bool isAdmin = false;

        if (userIdentityToken.decodedData<AnonymousIdentityToken>() != nullptr) {
            identityType = "Anonymous";
            std::cout << "ActivateSession: Anonymous user" << std::endl;

        } else if (const auto* usr = userIdentityToken.decodedData<UserNameIdentityToken>()) {
            identityType = "UserName";
            const auto user = usr->userName();
            // Example policy: username "admin" is admin
            isAdmin = (user == "admin");
            std::cout << "ActivateSession: Username user='" << user << "' isAdmin=" << isAdmin
                      << std::endl;

            // Store the provided username for later use
            session.setSessionAttribute({0, "userName"}, Variant{user});

        } else if (const auto* x509 = userIdentityToken.decodedData<X509IdentityToken>()) {
            identityType = "X509";
            // The certificate is in x509->certificateData() (ByteString)
            const auto certSize = x509->certificateData().size();
            std::cout << "ActivateSession: X509 user cert size=" << certSize << std::endl;
            // Example: grant elevated rights to any X509-authenticated user (for demo purposes)
            isAdmin = true;
            session.setSessionAttribute({0, "certSize"}, Variant{static_cast<uint32_t>(certSize)});

        } else if (const auto* iss = userIdentityToken.decodedData<IssuedIdentityToken>()) {
            identityType = "Issued";
            const auto tokenSize = iss->tokenData().size();
            const auto encAlg = iss->encryptionAlgorithm();
            std::cout << "ActivateSession: Issued token size=" << tokenSize
                      << ", encAlgo='" << encAlg << "'" << std::endl;
            // Example: accept issued tokens and grant elevated rights (for demo purposes)
            isAdmin = true;
            session.setSessionAttribute(
                {0, "issuedTokenSize"}, Variant{static_cast<uint32_t>(tokenSize)}
            );
            session.setSessionAttribute({0, "issuedEncAlgo"}, Variant{encAlg});

        } else {
            std::cout << "ActivateSession: Unsupported or unknown user token" << std::endl;
        }

        // Store normalized identity info into session attributes
        session.setSessionAttribute({0, "identityType"}, Variant{String{identityType}});
        session.setSessionAttribute({0, "isAdmin"}, Variant{isAdmin});

        // Delegate to the default implementation for actual verification/acceptance.
        // Note: AccessControlDefault typically supports Anonymous and Username. X509/Issued
        // acceptance depends on endpoint policies and your configuration.
        return AccessControlDefault::activateSession(
            session, endpointDescription, secureChannelRemoteCertificate, userIdentityToken
        );
    }

    void closeSession(Session& session) override {
        // Retrieve stored attributes if present
        std::string identityType{"Unknown"};
        std::string userNameStr;
        try {
            identityType = session.getSessionAttribute({0, "identityType"}).to<std::string>();
        } catch (...) {
        }
        try {
            userNameStr = session.getSessionAttribute({0, "userName"}).to<std::string>();
        } catch (...) {
        }

        std::cout << "CloseSession: id=" << opcua::toString(session.id())
                  << ", identityType=" << identityType;
        if (!userNameStr.empty()) {
            std::cout << ", userName='" << userNameStr << "'";
        }
        std::cout << std::endl;

        AccessControlDefault::closeSession(session);
    }

    Bitmask<AccessLevel> getUserAccessLevel(Session& session, const NodeId& nodeId) override {
        const auto identity = session.getSessionAttribute({0, "identityType"}).scalar<String>();
        // Try to print browse name of the node
        std::string browseNameStr, displayNameStr;
        try {
            // const auto qn = Node{session.connection(), nodeId}.readBrowseName();
            const auto qn = Node{session.connection(), nodeId}.readBrowseName();
            browseNameStr = std::string{"ns="} + std::to_string(qn.namespaceIndex()) +
                            ";name=" + std::string{qn.name()};

            const auto lt = Node{session.connection(), nodeId}.readDisplayName();
            displayNameStr = lt.text();
        } catch (...) {
            browseNameStr = "<unavailable>";
        }
        const bool admin = session.getSessionAttribute({0, "isAdmin"}).scalar<bool>();
        std::cout << "GetUserAccessLevel: node=" << opcua::toString(nodeId)
                  << " (browseName=" << browseNameStr << ")"
                  << ", identityType=" << identity << ", isAdmin=" << admin << " display: " << displayNameStr << std::endl;

        if (admin) {
            return AccessLevel::CurrentRead | AccessLevel::CurrentWrite;
        }
        return AccessLevel::CurrentRead;
    }
};

int main() {
    // WARNING: For demonstration only. Do not permit password exchange without security in
    // production. Configure your endpoints with appropriate SecurityPolicy and MessageSecurityMode.
    AccessControlAllTokens accessControl{
        true,  // allow anonymous
        {
            // simple demo user database for UserName tokens
            Login{String{"admin"}, String{"admin"}},
            Login{String{"user"}, String{"user"}},
        }
    };

    ServerConfig config;
    config.setAccessControl(accessControl);
#if UAPP_OPEN62541_VER_GE(1, 4)
    // Allow passwords with SecurityPolicy=None for demo purposes (not for production)
    config->allowNonePolicyPassword = true;
#endif

    Server server{std::move(config)};

    // Create a variable to test authorization differences between identities
    Node{server, ObjectId::ObjectsFolder}
        .addVariable(
            {1, 2001},
            "AuthzVariable",
            VariableAttributes{}
                .setAccessLevel(AccessLevel::CurrentRead | AccessLevel::CurrentWrite)
                .setDataType(DataTypeId::Int32)
                .setValueRank(ValueRank::Scalar)
                .setValue(opcua::Variant{0})
        );

    server.run();
}
