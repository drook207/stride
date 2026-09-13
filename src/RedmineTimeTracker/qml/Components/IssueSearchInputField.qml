import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QRedmine.TimeTracker

Item {
    id: searchField

    required property TimeTracker timeTracker

    property int value: -1

	readonly property bool acceptableInput: issueField.acceptableInput && searchField.value >= 0

    readonly property bool knownIssue: issueModel.hasIssue(value)

    signal accepted

    signal textChanged

    Item {

        IssueModel {
            id: issueModel
        }

        Connections {
            target: searchField.timeTracker
			function onIssueAddedOrUpdated() {
                issueModel.updateIssueList()
            }
        }

        QmlExposedSortFilterModelProxy {
            id: filteredIssuesModel
			filterRole: IssueModel.IdAndSubjectRole
            sourceModel: issueModel
            filterCaseSensitivity: Qt.CaseInsensitive
        }
    }

    TextField {
        id: issueField

        anchors.fill: parent
        font.pixelSize: 13

		function getAcceptableIssueIdFromInput() {
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

			searchField.textChanged()
		}

        enabled: !searchField.timeTracker.isTracking
		onTextChanged: issueField.getAcceptableIssueIdFromInput()
		onAccepted: {
			searchField.accepted()
			issueField.getAcceptableIssueIdFromInput()
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
