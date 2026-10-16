import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*!
 * \brief Bottom navigation bar for the mobile UI, replacing the desktop Sidebar.
 *
 * Unlike Sidebar (which special-cases "Settings" as an extra item appended after
 * buttonTitles.length), every tab here - including Settings - is a plain, uniformly indexed entry
 * in componentsToLoad/buttonTitles/navIcons; simpler since there's no expand/collapse state to
 * special-case around.
 */
Rectangle {
    id: docroot

    //! StackView whose current item this tab bar swaps via select().
    required property StackView view
    //! Component per tab, index-aligned with buttonTitles and navIcons.
    required property var componentsToLoad
    //! Label per tab, index-aligned with componentsToLoad and navIcons.
    required property var buttonTitles

    property int currentIndex: 0

    //! Icon filename per tab (under Assets/Icons/solid/), index-aligned with componentsToLoad and
    //! buttonTitles. Must be kept the same length as those two.
    readonly property var navIcons: ["clock-rotate-left.svg", "circle-plus.svg", "magnifying-glass.svg", "circle-plus.svg", "gear.svg"]

    implicitHeight: 64
    color: Material.backgroundColor

    //! Selects tab `index`: updates the highlighted tab and swaps the StackView's current item.
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
