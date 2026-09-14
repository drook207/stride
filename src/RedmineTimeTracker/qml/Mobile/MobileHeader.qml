import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QRedmine.Client

/*!
 * \brief Compact top bar for the mobile UI: app title and a small connection-state dot.
 *
 * Unlike the desktop Header, there is no hamburger button - mobile navigation is handled by
 * MobileTabBar at the bottom of the screen instead of a collapsible sidebar.
 */
Rectangle {
    id: header

    //! Mirrors RedmineClient::ConnectionState (ONLINE/TEMPORARY_OFFLINE/OFFLINE); drives the dot's
    //! color only (no text label, unlike the desktop Header's pill - there isn't room for one here).
    required property int connectionState

    height: 52
    color: Material.backgroundColor

    Text {
        anchors {
            left: parent.left
            leftMargin: 16
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: 18
        font.bold: true
        text: Application.name
        color: Material.foreground
    }

    Rectangle {
        anchors {
            right: parent.right
            rightMargin: 16
            verticalCenter: parent.verticalCenter
        }
        implicitWidth: 10
        implicitHeight: 10
        radius: 5
        color: {
            switch (header.connectionState) {
            case RedmineClient.ONLINE:
                return "#3DD68C"
            case RedmineClient.TEMPORARY_OFFLINE:
                return "#F5A623"
            default:
                return "#E5484D"
            }
        }
    }
}
