import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import drook207.timeTracker

Item {
    id: searchField

    property int value: -1

    readonly property bool acceptableInput: issueField.acceptableInput
                                            && searchField.value >= 0

    Item {

        IssueModel {
            id: issueModel
        }

        Connections {
            target: timeTrackerInstance
            function onIssueAddedOrUpdated() {
                console.log("Updating Issue list")
                issueModel.updateIssueList()
            }
        }

        QmlExposedSortFilterModelProxy {
            id: filteredIssuesModel
            filterRole: 258 // IdAndSubjectRole
            sourceModel: issueModel
            filterCaseSensitivity: Qt.CaseInsensitive
        }
    }

    TextField {
        id: issueField
        width: parent.width
        placeholderText: "Enter issue"
        enabled: !docroot.timeTracker.isTracking
        onTextChanged: {
            filteredIssuesModel.setFilterWildcard(issueField.text)
            searchDropDown.visible = issueField.text.length > 0

            if (acceptableInput) {
                const re = /^(\d+)\:?.*$/
                const ok = re.exec(issueField.text)
                if (ok) {
                    const result = issueField.text.match(re)
                    searchField.value = result[1] // Index 0 = Full String, Index 1 = Target Match
                } else {
                    console.log("Acceptable Input but regex could not find a match. This should not happen")
                    searchField.value = -1
                }
            } else {
                searchField.value = -1
            }
        }

        validator: IssueInputValidator {}
    }

    Popup {
        id: searchDropDown
        width: issueField.width
        height: suggestionListView.height
        x: issueField.x
        y: issueField.y + issueField.height
        modal: false

        padding: 0
        visible: false

        Rectangle {
            anchors.fill: parent

            color: Material.backgroundColor
            ListView {
                id: suggestionListView
                width: parent.width
                height: contentItem.childrenRect.height

                spacing: 0
                clip: true

                model: filteredIssuesModel

                delegate: Button {
                    required property string id
                    required property string subject
                    required property string idAndSubject
                    width: searchDropDown.width
                    height: 40
                    padding: 0
                    spacing: 0

                    Material.roundedScale: Material.NotRounded
                    contentItem: Text {
                        text: idAndSubject
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: Text.AlignLeft
                        anchors.left: parent.left
                        anchors.leftMargin: 10 // Optional: Abstandsraum von der linken Seite
                        color: Material.foreground
                    }

                    onClicked: {
                        issueField.text = idAndSubject
                        searchField.value = id
                        searchDropDown.visible = false
                    }
                }
            }
        }
    }
}
