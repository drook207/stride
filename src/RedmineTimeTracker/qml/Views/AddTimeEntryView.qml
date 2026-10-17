import QtQuick
import QtQml
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief "Add Entry" view: manually logs a time entry without using the live start/stop tracker -
 * useful for backfilling work done earlier.
 *
 * Uses its own AddDialog confirmation step, which is why this flow has a separate (near-
 * identical) dialog from the live TrackingBar/MobileTrackingBar's SubmitDialog: AddDialog takes
 * hours/spentOn as plain properties set by this view, while SubmitDialog reads them live from
 * TimeTracker's running timer.
 */
Item {
    id: docroot
    property TimeTracker timeTracker: undefined
    property RedmineClient client: undefined
    //! TimeEntryActivitiesModel providing the activities for the Activity combo box.
    property var activitiesModel: undefined
    //! The time entry custom field "Special" (Main.qml), or null if the server has none.
    property var specialField: null
    property AddDialog addDialog: AddDialog {
        client: docroot.client
        timeTracker: docroot.timeTracker
    }
    //! Copies the current field values into addDialog. timeSpentField is parsed by HoursFormat
    //! (decimal comma, e.g. "1,5"); hours that don't parse are left as they were.
    function saveInput() {
        docroot.addDialog.spentOn = spentOnField.isoDate
        docroot.addDialog.issueId = issueField.value
        const number = HoursFormat.parse(timeSpentField.text)
        if (!isNaN(number)) {
            docroot.addDialog.hours = number
        }
    }

    Timer {
        id: inputValidatorTimer
        interval: 1000
        onTriggered: {

            docroot.saveInput()
        }
    }

    FormPage {
        anchors.fill: parent
        title: qsTr("Add Time Entry")

        Card {
            Layout.fillWidth: true

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Issue")

                IssueSearchInputField {
                    id: issueField
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.controlHeight
                    timeTracker: docroot.timeTracker

                    onAccepted: {
                        if (acceptableInput) {
                            docroot.addDialog.issueId = issueField.value
                        }
                    }

                    onTextChanged: {
                        inputValidatorTimer.restart()
                    }
                }
            }

            // Optional helper: picking a time range fills in "Time spent". Redmine only stores
            // the hours, so the range itself isn't submitted.
            TimeRangeRow {
                Layout.fillWidth: true
                onDurationPicked: hours => timeSpentField.text = HoursFormat.format(hours)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Time spent (eg. 1,0)")

                    TextField {
                        id: timeSpentField
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        validator: DoubleValidator {
                            bottom: 0
                            decimals: 2
                            notation: DoubleValidator.StandardNotation
                        }
                        onAccepted: {
                            if (acceptableInput) {
                                docroot.addDialog.hours = HoursFormat.parse(text)
                            }
                        }
                        onTextChanged: {
                            inputValidatorTimer.restart()
                        }
                    }
                }

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Date")

                    DatePickerField {
                        id: spentOnField
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        // Today until another date is picked, so addDialog always gets a date.
                        Component.onCompleted: docroot.addDialog.spentOn = spentOnField.isoDate
                        onIsoDateChanged: docroot.addDialog.spentOn = spentOnField.isoDate
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Activity")

                    ActivityComboBox {
                        id: activityCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        activitiesModel: docroot.activitiesModel
                        projectId: issueField.projectId
                        // Submit the activity that is shown; 0 (Redmine's default) while none are loaded yet.
                        onActivityIdChanged: docroot.addDialog.activityId = activityId
                        Component.onCompleted: docroot.addDialog.activityId = activityId
                    }
                }

                // Redmine's "Special" time entry custom field; hidden while the server has none.
                LabeledField {
                    Layout.fillWidth: true
                    visible: docroot.specialField !== null
                    label: qsTr("Special")

                    SpecialFieldComboBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        field: docroot.specialField
                        onCustomFieldValuesChanged: docroot.addDialog.customFields = customFieldValues
                        Component.onCompleted: docroot.addDialog.customFields = customFieldValues
                    }
                }
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Comments")

                TextArea {
                    id: commentsField
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.textAreaHeight
                    wrapMode: TextEdit.Wrap
                }
            }

            PrimaryButton {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 4
                text: qsTr("Add")
                onClicked: {
                    docroot.addDialog.comment = commentsField.text
                    docroot.addDialog.open()
                }
            }
        }
    }
}
