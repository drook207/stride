import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QRedmine.TimeTracker
import QRedmine.Client
import QtCore

/*!
 * \brief Connection settings: Redmine URL, API key, and SSL toggle.
 */
Item {
    id: docroot
    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog
    required property Reminders reminders
    //! Whether the platform offers a system tray, for the close-to-tray option.
    property bool trayAvailable: false

    // Read by Main.qml when the window is closed.
    Settings {
        id: traySettings
        category: "tray"
        property bool closeToTray: false
    }

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
                    // Not on every keystroke: setting the key stores it and refetches everything.
                    // Applied once typing pauses, or at once when confirmed.
                    onTextChanged: apiKeyTimer.restart()
                    onEditingFinished: {
                        apiKeyTimer.stop();
                        apiKeyTimer.apply();
                    }
                    Timer {
                        id: apiKeyTimer
                        interval: 1500
                        function apply() {
                            if (parent.text !== docroot.client.apiKey) {
                                docroot.client.apiKey = parent.text;
                            }
                        }
                        onTriggered: apply()
                    }
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

        Card {
            Layout.fillWidth: true
            title: qsTr("Reminders")

            Switch {
                Layout.leftMargin: -8
                text: qsTr("Remind me of unbooked time and long-running timers")
                checked: docroot.reminders.enabled
                onToggled: docroot.reminders.enabled = checked
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: docroot.reminders.enabled
                spacing: 12

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Nothing booked on a workday by (HH:mm)")

                    TextField {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        text: docroot.reminders.endOfDay
                        inputMask: "99:99"
                        onEditingFinished: docroot.reminders.endOfDay = text
                    }
                }

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Timer running for (hours, 0 = never)")

                    SpinBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        from: 0
                        to: 24
                        editable: true
                        value: docroot.reminders.longTrackingHours
                        onValueModified: docroot.reminders.longTrackingHours = value
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            visible: docroot.trayAvailable
            title: qsTr("System tray")

            Switch {
                Layout.leftMargin: -8
                text: qsTr("Keep running in the tray when the window is closed")
                checked: traySettings.closeToTray
                onToggled: traySettings.closeToTray = checked
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
