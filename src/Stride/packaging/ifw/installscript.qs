/*
 * Qt Installer Framework component script for the Stride component (see the CPack
 * section of src/Stride/CMakeLists.txt). Adds the platform's application menu entry;
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
            "@TargetDir@/bin/Stride.exe",
            "@StartMenuDir@/Stride.lnk",
            "workingDirectory=@TargetDir@/bin",
            "iconPath=@TargetDir@/bin/Stride.exe",
            "iconId=0",
            "description=Time tracker for Redmine issues");
        component.addOperation("CreateShortcut",
            "@TargetDir@/@MaintenanceToolName@.exe",
            "@StartMenuDir@/Uninstall Stride.lnk",
            "workingDirectory=@TargetDir@",
            "description=Uninstall or update Stride");
    } else if (systemInfo.kernelType === "linux") {
        // Per-user entry, matching the per-user default install location (@HomeDir@/...).
        // CreateDesktopEntry doesn't create missing parent directories, which a fresh profile may lack.
        component.addOperation("Mkdir", "@HomeDir@/.local/share/applications");
        component.addOperation("CreateDesktopEntry",
            "@HomeDir@/.local/share/applications/io.github.drook207.Stride.desktop",
            "Type=Application\n"
            + "Name=Stride\n"
            + "Comment=Time tracker for Redmine issues\n"
            + "Comment[de]=Zeiterfassung für Redmine-Tickets\n"
            + "Exec=\"@TargetDir@/bin/Stride\"\n"
            + "Path=@TargetDir@/bin\n"
            + "Icon=@TargetDir@/share/icons/hicolor/scalable/apps/io.github.drook207.Stride.svg\n"
            + "Terminal=false\n"
            + "Categories=Office;ProjectManagement;Utility;\n");
    }
}
