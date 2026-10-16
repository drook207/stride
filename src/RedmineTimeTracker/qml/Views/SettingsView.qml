import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Connection settings: Redmine URL, API key, and SSL toggle.
 *
 * Note: the fields below bind to the bare id `redmineClient`, not to the `client` property
 * declared here - that id is declared in Main.qml and resolves through the ambient QML context
 * chain (Main.qml -> [Desktop|Mobile]Shell -> StackView -> this view), which happens to work but
 * is fragile: it would silently break if this view were ever instantiated outside that chain. The
 * `client` property is unused; prefer `docroot.client` for any new bindings here.
 */
Item {
    id: docroot
    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog

    FormPage {
        anchors.fill: parent
        title: qsTr("Settings")

        Card {
            Layout.fillWidth: true
            title: qsTr("Connection Settings")

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Redmine URL")

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.controlHeight
                    text: redmineClient.baseUrl
                    validator: RegularExpressionValidator {
                        regularExpression: /^https?:\/\/[^\s\/$.?#].[^\s]*$/
                    }
                    onAccepted: {
                        console.log("Valid URL entered: " + text);
                        if (text.endsWith("/")) {
                            redmineClient.baseUrl = text;
                        } else {
                            redmineClient.baseUrl = text + "/";
                        }
                    }
                }
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("API Key")

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.controlHeight
                    text: redmineClient.apiKey
                    onTextChanged: redmineClient.apiKey = text
                    echoMode: TextInput.Password
                }
            }

            CheckBox {
                Layout.leftMargin: -8
                text: qsTr("Use SSL")
                checked: redmineClient.useSSL
                onCheckedChanged: redmineClient.useSSL = checked
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Appearance")

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Language")

                ComboBox {
                    id: languageComboBox
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.controlHeight
                    topInset: 0
                    bottomInset: 0
                    textRole: "name"
                    valueRole: "code"
                    // "System default" (empty code) first, then one entry per embedded catalog. Language
                    // names are shown in their own language, so only the first entry is translated.
                    model: [{
                            "code": "",
                            "name": qsTr("System default")
                        }].concat(LanguageManager.availableLanguages)
                    // Re-sync after the model is rebuilt on retranslation, which resets currentIndex.
                    onModelChanged: currentIndex = indexOfValue(LanguageManager.language)
                    Component.onCompleted: currentIndex = indexOfValue(LanguageManager.language)
                    onActivated: LanguageManager.language = currentValue
                }
            }
        }

        Label {
            Layout.leftMargin: 4
            text: qsTr("Version %0").arg(Application.version)
            color: Material.hintTextColor
            font.pixelSize: 11
        }
    }
}
