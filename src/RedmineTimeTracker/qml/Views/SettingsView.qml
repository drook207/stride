import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Connection settings: Redmine URL, API key, and SSL toggle.
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
                    text: docroot.client.baseUrl
                    validator: RegularExpressionValidator {
                        regularExpression: /^https?:\/\/[^\s\/$.?#].[^\s]*$/
                    }
                    onAccepted: {
                        console.log("Valid URL entered: " + text);
                        if (text.endsWith("/")) {
                            docroot.client.baseUrl = text;
                        } else {
                            docroot.client.baseUrl = text + "/";
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
                    text: docroot.client.apiKey
                    onTextChanged: docroot.client.apiKey = text
                    echoMode: TextInput.Password
                }
            }

            CheckBox {
                Layout.leftMargin: -8
                text: qsTr("Use SSL")
                checked: docroot.client.useSSL
                onCheckedChanged: docroot.client.useSSL = checked
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
