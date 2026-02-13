#include "../src/fallback/prompt/PromptModelBuilder.hpp"
#include "../src/fallback/prompt/PromptModel.hpp"

#include <QtTest/QtTest>

namespace bb {

    class PromptModelBuilderTouchTest : public QObject {
        Q_OBJECT

      private slots:
        void fingerprintInfoClassifiesAsTouchAuth();
        void securityKeyInfoClassifiesAsTouchAuth();
        void plainPolkitPromptRequiresPassword();
        void pinentryPromptRemainsPassphraseDriven();

      private:
        static QJsonObject makeEvent(const QString& source, const QString& message, const QString& info = QString());
    };

    QJsonObject PromptModelBuilderTouchTest::makeEvent(const QString& source, const QString& message, const QString& info) {
        QJsonObject context{{"message", message}, {"requestor", QJsonObject{{"name", "test-app"}}}};
        QJsonObject event{{"type", "session.created"}, {"id", "session-1"}, {"source", source}, {"context", context}};
        if (!info.isEmpty()) {
            event.insert("info", info);
        }
        return event;
    }

    void PromptModelBuilderTouchTest::fingerprintInfoClassifiesAsTouchAuth() {
        const fallback::prompt::PromptModelBuilder builder;
        const QJsonObject                          event = makeEvent("polkit", "Authentication is required", "Swipe your fingerprint sensor");
        const auto                                 model = builder.build(event);

        QCOMPARE(model.intent, fallback::prompt::PromptIntent::Fingerprint);
        QVERIFY(model.allowEmptyResponse);
        QCOMPARE(model.prompt, QString("Press Enter to continue (or wait)"));
    }

    void PromptModelBuilderTouchTest::securityKeyInfoClassifiesAsTouchAuth() {
        const fallback::prompt::PromptModelBuilder builder;
        const QJsonObject                          event = makeEvent("polkit", "Authentication is required", "Touch your security key to continue");
        const auto                                 model = builder.build(event);

        QCOMPARE(model.intent, fallback::prompt::PromptIntent::Fido2);
        QVERIFY(model.allowEmptyResponse);
        QCOMPARE(model.prompt, QString("Press Enter to continue (or wait)"));
    }

    void PromptModelBuilderTouchTest::plainPolkitPromptRequiresPassword() {
        const fallback::prompt::PromptModelBuilder builder;
        const QJsonObject                          event = makeEvent("polkit", "Authentication is required to install software");
        const auto                                 model = builder.build(event);

        QCOMPARE(model.prompt, QString("Password:"));
        QVERIFY(!model.allowEmptyResponse);
    }

    void PromptModelBuilderTouchTest::pinentryPromptRemainsPassphraseDriven() {
        const fallback::prompt::PromptModelBuilder builder;

        const QJsonObject                          context{{"message", ""}, {"description", "Unlock OpenPGP secret key"}, {"requestor", QJsonObject{{"name", "gpg"}}}};
        const QJsonObject                          event{{"type", "session.created"}, {"id", "session-2"}, {"source", "pinentry"}, {"context", context}};

        const auto                                 model = builder.build(event);

        QCOMPARE(model.prompt, QString("Passphrase:"));
        QVERIFY(!model.allowEmptyResponse);
    }

} // namespace bb

int runFallbackWindowTouchModelTests(int argc, char** argv) {
    bb::PromptModelBuilderTouchTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_fallback_touch_model.moc"
