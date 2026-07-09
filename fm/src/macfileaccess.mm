#include "macfileaccess.h"

#ifdef Q_OS_MAC

#import <AppKit/AppKit.h>

namespace MacFileAccess {

static void openSettingsURL(NSString *urlString)
{
    NSURL *url = [NSURL URLWithString:urlString];
    if (!url) {
        return;
    }
    [[NSWorkspace sharedWorkspace] openURL:url];
}

void openFullDiskAccessSettings()
{
    if (@available(macOS 13.0, *)) {
        openSettingsURL(@"x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_AllFiles");
    } else {
        openSettingsURL(@"x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles");
    }
}

void openFilesAndFoldersSettings()
{
    if (@available(macOS 13.0, *)) {
        openSettingsURL(@"x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_Files");
    } else {
        openSettingsURL(@"x-apple.systempreferences:com.apple.preference.security?Privacy_FilesAndFolders");
    }
}

void setApplicationAppearance(bool darkUi)
{
    if (@available(macOS 10.14, *)) {
        NSAppearanceName name = darkUi ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua;
        NSAppearance *appearance = [NSAppearance appearanceNamed:name];
        if (appearance) {
            [NSApp setAppearance:appearance];
        }
    }
}

} // namespace MacFileAccess

#endif
