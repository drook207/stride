# Third-party notices

Stride is licensed under the MIT License, see
[LICENSE](LICENSE). Its binary packages include the third-party software below,
each under its own license. The full license texts are in [LICENSES/](LICENSES/).

| Component                                                         | Used for                                                                         | License       | License text                                                                                   |
| ----------------------------------------------------------------- | -------------------------------------------------------------------------------- | ------------- | ---------------------------------------------------------------------------------------------- |
| [Qt 6](https://www.qt.io)                                         | Application framework (Core, Gui, Network, Qml, Quick, Quick Controls, Sql, Svg) | LGPL-3.0-only | [LGPL-3.0-only.txt](LICENSES/LGPL-3.0-only.txt), [GPL-3.0-only.txt](LICENSES/GPL-3.0-only.txt) |
| [QtKeychain](https://github.com/frankosterfeld/qtkeychain) 0.17.0 | Storing the API key in the OS keychain                                           | BSD-3-Clause  | [BSD-3-Clause-QtKeychain.txt](LICENSES/BSD-3-Clause-QtKeychain.txt)                            |
| [Font Awesome Free](https://fontawesome.com) 6.7.2 icons          | Icons in the UI                                                                  | CC-BY-4.0     | [CC-BY-4.0.txt](LICENSES/CC-BY-4.0.txt)                                                        |
| [Font Awesome Free](https://fontawesome.com) 6 Regular font       | Icon font in the UI                                                              | OFL-1.1       | [OFL-1.1.txt](LICENSES/OFL-1.1.txt)                                                            |

## Qt

Qt is copyright The Qt Company Ltd. and other contributors, and used here under
the GNU Lesser General Public License version 3 (LGPL-3.0-only). The LGPL-3.0 adds
permissions to the GNU General Public License version 3, so both texts are
included.

- Stride links Qt dynamically: the Qt libraries ship as separate files next to
    the application, and you may replace them with your own, modified build of the
    same Qt version.
- The source code of Qt is available at <https://download.qt.io/official_releases/qt/>
    and <https://code.qt.io>; use the Qt version the package was built with.
- Qt itself contains third-party code under further licenses. They are listed at
    <https://doc.qt.io/qt-6/licenses-used-in-qt.html>.

## Font Awesome

Font Awesome Free by Fonticons, Inc., <https://fontawesome.com/license/free>:
the icons are licensed under CC BY 4.0, the font under the SIL Open Font License
1.1. The icons are used unchanged; some are recolored at runtime.
