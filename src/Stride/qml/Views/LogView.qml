pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import Stride

/*!
 * \brief Full-window viewer for the application log (see LogModel): the latest messages, filterable
 * by severity, category and text.
 *
 * A popup over the whole window rather than a page, so it works the same with the desktop sidebar and the
 * mobile tab bar. Open it with open(); Escape or "Close" dismisses it. Tapping a row expands it (the whole
 * message and fields, and a button to copy it as JSON).
 */
Popup {
    id: docroot

    //! The messages listed, filtered by the controls below.
    readonly property LogModel logModel: LogModel {}
    //! Whether the list stays at the newest message.
    property bool follow: true
    //! Sequence number of the entry expanded to show everything, -1 for none. Not a row index: those
    //! shift when the model trims old rows or the filter changes.
    property real expandedSequence: -1

    function levelColor(level) {
        switch (level) {
        case "debug":
            return Material.hintTextColor;
        case "warning":
            return Material.color(Material.Orange, Material.Shade200);
        case "critical":
        case "fatal":
            return Material.color(Material.Red, Material.Shade200);
        default:
            return Material.secondaryTextColor;
        }
    }

    function scrollToEnd() {
        if (docroot.follow) {
            listView.positionViewAtEnd();
        }
    }

    parent: Overlay.overlay
    width: parent.width
    height: parent.height
    padding: 12
    modal: true
    focus: true
    Overlay.modal: DialogScrim {}
    background: Rectangle {
        color: Material.backgroundColor
    }

    onAboutToShow: {
        docroot.expandedSequence = -1;
    }
    onOpened: {
        docroot.follow = true;
        Qt.callLater(docroot.scrollToEnd);
    }

    // Copies text to the clipboard: QML has no API for it, a hidden TextEdit can.
    TextEdit {
        id: clipboard
        visible: false
        function copyText(value) {
            text = value;
            selectAll();
            copy();
        }
    }

    Timer {
        id: searchTimer
        interval: 250
        onTriggered: docroot.logModel.searchText = searchField.text
    }

    Connections {
        target: docroot.logModel
        function onCountChanged() {
            Qt.callLater(docroot.scrollToEnd);
        }
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            PageTitle {
                Layout.fillWidth: true
                text: qsTr("Log")
            }

            Label {
                text: qsTr("%n message(s)", "", docroot.logModel.count)
                color: Material.hintTextColor
                font.pixelSize: 12
            }

            Button {
                text: qsTr("Close")
                flat: true
                onClicked: docroot.close()
            }
        }

        // Wraps below one line on narrow windows.
        Flow {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: levelCombo
                width: 150
                height: Metrics.controlHeight
                topInset: 0
                bottomInset: 0
                textRole: "text"
                valueRole: "value"
                model: [{
                        "text": qsTr("Debug and up"),
                        "value": LogModel.Debug
                    }, {
                        "text": qsTr("Info and up"),
                        "value": LogModel.Info
                    }, {
                        "text": qsTr("Warnings and up"),
                        "value": LogModel.Warning
                    }, {
                        "text": qsTr("Errors only"),
                        "value": LogModel.Critical
                    }]
                onActivated: docroot.logModel.minimumLevel = currentValue
            }

            ComboBox {
                id: categoryCombo
                width: 170
                height: Metrics.controlHeight
                topInset: 0
                bottomInset: 0
                // The first entry stands for every category.
                model: [qsTr("All categories")].concat(docroot.logModel.categories)
                // Re-sync after the list grows, which resets currentIndex.
                onModelChanged: currentIndex = Math.max(0, find(docroot.logModel.category))
                onActivated: docroot.logModel.category = currentIndex === 0 ? "" : currentText
            }

            TextField {
                id: searchField
                width: 200
                height: Metrics.controlHeight
                placeholderText: qsTr("Search")
                onTextChanged: searchTimer.restart()
            }

            Switch {
                text: qsTr("Follow")
                checked: docroot.follow
                onToggled: {
                    docroot.follow = checked;
                    docroot.scrollToEnd();
                }
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: docroot.logModel
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            // Scrolling away from the newest messages by hand stops following.
            onDragStarted: docroot.follow = false

            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: entry
                required property int index
                required property date ts
                required property string level
                required property string category
                required property string message
                required property string fields
                required property real sequence
                readonly property bool expanded: docroot.expandedSequence === sequence

                width: ListView.view.width
                implicitHeight: content.implicitHeight + 12
                height: implicitHeight
                color: expanded ? Material.dialogColor : "transparent"

                // Collapse when the entry leaves the list (trimmed or filtered out, or scrolled far away).
                ListView.onPooled: if (expanded) {
                    docroot.expandedSequence = -1;
                }
                Component.onDestruction: if (expanded) {
                    docroot.expandedSequence = -1;
                }

                TapHandler {
                    onTapped: docroot.expandedSequence = entry.expanded ? -1 : entry.sequence
                }

                ColumnLayout {
                    id: content
                    x: 6
                    y: 6
                    width: parent.width - 12
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: Qt.formatTime(entry.ts, "HH:mm:ss.zzz")
                            color: Material.hintTextColor
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        Rectangle {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 16
                            radius: 4
                            color: "transparent"
                            border.color: docroot.levelColor(entry.level)

                            Label {
                                anchors.centerIn: parent
                                text: entry.level.toUpperCase()
                                color: docroot.levelColor(entry.level)
                                font.pixelSize: 10
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: entry.category
                            color: Material.secondaryTextColor
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Button {
                            visible: entry.expanded
                            text: qsTr("Copy")
                            flat: true
                            Layout.preferredHeight: 28
                            topInset: 0
                            bottomInset: 0
                            onClicked: clipboard.copyText(docroot.logModel.rowAsJson(entry.index))
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: entry.message
                        color: Material.foreground
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        maximumLineCount: entry.expanded ? 1000 : 2
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: entry.fields.length > 0
                        text: entry.fields
                        color: Material.hintTextColor
                        font.pixelSize: 11
                        wrapMode: entry.expanded ? Text.WrapAnywhere : Text.NoWrap
                        elide: Text.ElideRight
                        maximumLineCount: entry.expanded ? 1000 : 1
                    }
                }
            }
        }
    }
}
