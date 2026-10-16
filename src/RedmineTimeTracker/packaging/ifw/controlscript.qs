/*
 * Qt Installer Framework control script (see the CPack section of
 * src/RedmineTimeTracker/CMakeLists.txt).
 */

function Controller()
{
    // The installer contains a single, mandatory component, so there is nothing to select.
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
}
