import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QRedmine.TimeTracker
import QRedmine.Client

Item {
    id: docroot
    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog

    ColumnLayout {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 15
        }
        spacing: 15

        Label {
            text: qsTr("Settings")
            font.bold: true
            font.pixelSize: 18
            color: Material.foreground
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: qsTr("Connection Settings")
                font.bold: true
                font.pixelSize: 14
                color: Material.foreground
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Redmine URL")

                TextField {
                    Layout.fillWidth: true
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
                    text: redmineClient.apiKey
                    onTextChanged: redmineClient.apiKey = text
                    echoMode: TextInput.Password
                }
            }
        }

        CheckBox {
            text: qsTr("Use SSL")
            checked: redmineClient.useSSL
            onCheckedChanged: redmineClient.useSSL = checked
        }

        Label {
            text: qsTr("Version %0").arg(Application.version)
            color: Material.hintTextColor
            font.pixelSize: 11
            Layout.topMargin: 20
        }
    }
}
