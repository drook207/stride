/*
 * Qt Installer Framework component script for the QRedmineTimeTracker component (see the CPack
 * section of src/RedmineTimeTracker/CMakeLists.txt). Adds the platform's application menu entry;
 * operations added here are reverted automatically on uninstall.
 */

function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/bin/QRedmineTimeTracker.exe",
            "@StartMenuDir@/QRedmineTimeTracker.lnk",
            "workingDirectory=@TargetDir@/bin",
            "iconPath=@TargetDir@/bin/QRedmineTimeTracker.exe",
            "iconId=0",
            "description=Time tracker for Redmine issues");
        component.addOperation("CreateShortcut",
            "@TargetDir@/@MaintenanceToolName@.exe",
            "@StartMenuDir@/Uninstall QRedmineTimeTracker.lnk",
            "workingDirectory=@TargetDir@",
            "description=Uninstall or update QRedmineTimeTracker");
    } else if (systemInfo.kernelType === "linux") {
        // Per-user entry, matching the per-user default install location (@HomeDir@/...).
        // CreateDesktopEntry doesn't create missing parent directories, which a fresh profile may lack.
        component.addOperation("Mkdir", "@HomeDir@/.local/share/applications");
        component.addOperation("CreateDesktopEntry",
            "@HomeDir@/.local/share/applications/qredminetimetracker.desktop",
            "Type=Application\n"
            + "Name=QRedmineTimeTracker\n"
            + "Comment=Time tracker for Redmine issues\n"
            + "Comment[de]=Zeiterfassung für Redmine-Tickets\n"
            + "Exec=\"@TargetDir@/bin/QRedmineTimeTracker\"\n"
            + "Path=@TargetDir@/bin\n"
            + "Icon=@TargetDir@/share/icons/hicolor/scalable/apps/qredminetimetracker.svg\n"
            + "Terminal=false\n"
            + "Categories=Office;ProjectManagement;Utility;\n");
    }
}
