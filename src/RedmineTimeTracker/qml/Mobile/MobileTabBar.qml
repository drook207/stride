import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: docroot

    required property StackView view
    required property var componentsToLoad
    required property var buttonTitles

    property int currentIndex: 0

    readonly property var navIcons: ["clock-rotate-left.svg", "circle-plus.svg", "magnifying-glass.svg", "circle-plus.svg", "gear.svg"]

    implicitHeight: 64
    color: Material.backgroundColor

    function select(index) {
        docroot.currentIndex = index
        docroot.view.replaceCurrentItem(docroot.componentsToLoad[index])
    }

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: 1
        color: Material.dividerColor
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: docroot.buttonTitles

            ItemDelegate {
                id: navItem
                required property int index

                Layout.fillWidth: true
                Layout.fillHeight: true

                checked: docroot.currentIndex === index
                display: AbstractButton.TextUnderIcon
                text: docroot.buttonTitles[index]
                font.pixelSize: 10
                icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/" + docroot.navIcons[index]
                icon.width: 20
                icon.height: 20
                icon.color: navItem.checked ? Material.accentColor : Material.hintTextColor

                background: Rectangle {
                    color: navItem.checked ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                }

                onClicked: docroot.select(index)
            }
        }
    }
}
