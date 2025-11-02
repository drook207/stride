import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import drook207.timeTracker
import drook207.redmineClient

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
        }
        GroupBox {
			title: qsTr("Connection Settings")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                rowSpacing: 10
                columnSpacing: 10
                anchors.fill: parent

                Label {
                    text: qsTr("Redmine URL:")
                }
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

                    placeholderText: qsTr("Enter your Redmine URL")
                }

                Label {
                    text: qsTr("API Key:")
                }
                TextField {
                    Layout.fillWidth: true
                    text: redmineClient.apiKey
                    onTextChanged: redmineClient.apiKey = text
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Enter your API key")
                }
            }
        }
    }
}
