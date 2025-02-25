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
            title: "Connection Settings"
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                rowSpacing: 10
                columnSpacing: 10
                anchors.fill: parent

                Label {
                    text: "Redmine URL:"
                }
                TextField {
                    Layout.fillWidth: true
                    text: redmineClient.baseUrl
                    onTextChanged: redmineClient.baseUrl = text
                    placeholderText: "Enter your Redmine URL"
                }

                Label {
                    text: "API Key:"
                }
                TextField {
                    Layout.fillWidth: true
                    text: redmineClient.apiKey
                    onTextChanged: redmineClient.apiKey = text
                    echoMode: TextInput.Password
                    placeholderText: "Enter your API key"
                }
            }
        }
    }
}
