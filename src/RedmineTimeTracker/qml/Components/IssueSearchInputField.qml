import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QRedmine.TimeTracker

/*!
 * \brief Issue-number input field with live autocomplete against the locally cached issue list.
 *
 * Used wherever the user needs to pick an existing issue: TrackingBar, MobileTrackingBar,
 * AddTimeEntryView, and GetTimeEntriesView. Text can be typed as a bare id ("42") or picked from
 * the dropdown ("42: Some subject"); either way, `value` ends up holding the parsed numeric id.
 */
Item {
    id: searchField

    required property TimeTracker timeTracker

    //! The parsed issue id, or -1 if the current text doesn't resolve to one. This is what
    //! callers should read/bind to, not the raw displayed text.
    property int value: -1

    //! True once the current text parses to a non-negative id per the IssueInputValidator and
    //! the "<id>[:...]" regex below - independent of whether that id is actually known locally.
	readonly property bool acceptableInput: issueField.acceptableInput && searchField.value >= 0

    //! True if `value` refers to an issue already present in the local issue cache (i.e. one the
    //! app has previously fetched via RedmineClient::getIssue). Callers use this to gate actions
    //! that need issue details (e.g. starting time tracking) on the issue actually being known.
    readonly property bool knownIssue: issueModel.hasIssue(value)

    //! Emitted when Enter/Return is pressed in the field (mirrors TextField.accepted).
    signal accepted

    //! Emitted after `value` has been re-derived from the current text (mirrors TextField's own
    //! textChanged, but fires after value/acceptableInput/knownIssue are already up to date).
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

		//! Re-filters the suggestion dropdown from the current text and re-derives
		//! searchField.value: extracts the leading digits (before an optional ":") if the text is
		//! acceptable input, otherwise resets value to -1. Called on every keystroke.
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
        // At least wide enough for a subject next to its project, even in the narrow tracking bar.
        width: Math.max(issueField.width, 340)
        // Scrolls beyond a few suggestions rather than growing past the window.
        height: Math.min(suggestionListView.contentHeight, 320)
        x: issueField.x
        y: openAbove ? issueField.y - height : issueField.y + issueField.height
        modal: false

        //! Whether the list opens above the field, because there isn't enough room below it, e.g.
        //! in the tracking bar at the bottom of the window. Re-evaluated whenever the list or the
        //! window changes size.
        readonly property bool openAbove: visible && searchField.Window.height > 0
                                          && issueField.mapToItem(null, 0, issueField.height).y + height
                                          > searchField.Window.height

        padding: 0
        visible: false

        Rectangle {
            anchors.fill: parent

            color: Material.backgroundColor
            ListView {
                id: suggestionListView
                anchors.fill: parent

                spacing: 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                model: filteredIssuesModel

                // IssueModel is sorted by project, so each project's issues form one section.
                section.property: "projectName"
                section.criteria: ViewSection.FullString
                section.delegate: Label {
                    required property string section
                    width: ListView.view.width
                    leftPadding: 10
                    topPadding: 8
                    bottomPadding: 4
                    text: section.length > 0 ? section : qsTr("No project")
                    color: Material.accentColor
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

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
