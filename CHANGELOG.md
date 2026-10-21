# Changelog

All notable changes to Stride. Generated from the commit messages by
[git-cliff](https://git-cliff.org); the project uses
[Semantic Versioning](https://semver.org/).

## [0.2.0-beta.1](https://github.com/drook207/stride/releases/tag/v0.2.0-beta.1) - 2026-10-21

### Breaking changes

- Rename the application to Stride ([c7a420b](https://github.com/drook207/stride/commit/c7a420bd04285c88ddbaf119db926ad8c33df00c))
- Move to GitHub ([a2ba25b](https://github.com/drook207/stride/commit/a2ba25b0c91ba9489f722ec548d94c66aeb1b92a))

### Features

- **logging:** Log to rotating JSON Lines files, with a log view in the app ([1124270](https://github.com/drook207/stride/commit/1124270bf73bbc04314c0b733d76763e28577a11))
- **tracking:** Pick the start time of a running timer, clear the bar after submitting ([4b48444](https://github.com/drook207/stride/commit/4b48444e5f487f16de2141d95ee859741bfa962e))

### Bug fixes

- **client:** Stop flooding Redmine with connection checks, time out requests ([ff330c1](https://github.com/drook207/stride/commit/ff330c1d967f5360e9dc43e7d1b0ff58bdd1d0fc))
- **sync:** Send less to Redmine and back off when it struggles ([4639708](https://github.com/drook207/stride/commit/46397086de9246514f5b0c642b25db4136d6ef11))
- **tray:** Create the tray menu where the platform has no native one ([f9dc628](https://github.com/drook207/stride/commit/f9dc628a95e575a2eecdd73f229dd1a222fd5a0a))
- **startup:** Wait for a server URL before the startup requests ([523d641](https://github.com/drook207/stride/commit/523d6411db70f82d8dde3dfcff43c8704163062f))
- **tests:** Compare the log limits by value ([cf18ee1](https://github.com/drook207/stride/commit/cf18ee16f6175739841c665c5c90e538cdfa1417))
- **tests:** Disconnect failCreateWith()'s lambda when it returns ([ca6f3bf](https://github.com/drook207/stride/commit/ca6f3bf55775d67845dcab1c1ec261ddc1749ed9))
- **ui:** Open the time picker above the field near the window's bottom ([786115f](https://github.com/drook207/stride/commit/786115f206be0eebb0e70d7ee72554b10bb3b812))
- **ui:** Show the issue in the issue field when it's set from outside ([d4e9a9e](https://github.com/drook207/stride/commit/d4e9a9e5b15962ec1b596f69592a8f6e69a73cff))
- **client:** No error dialog at startup when the server can't be reached ([a7cd7f1](https://github.com/drook207/stride/commit/a7cd7f1e09b4b3fc638c7d5237dbb8bf4d2d5afb))
- **migration:** Take over macOS settings, say when a database is kept ([7fb4627](https://github.com/drook207/stride/commit/7fb4627d48f31ecaba8b1fef96c7b71b75d544b3))

## [0.1.0](https://github.com/drook207/stride/releases/tag/v0.1.0) - 2026-10-18

### Features

- **redmine-client:** Add issue and project CRUD operations ([fcc0c45](https://github.com/drook207/stride/commit/fcc0c45ca8c03bc3c326f3e0a57ffe375c527179))
- **redmine-client:** Detect Redmine REST API version ([44a5fc1](https://github.com/drook207/stride/commit/44a5fc111016c198c3eaf84d9d433842618ebb3d))
- **security:** Store Redmine API key in OS-native secure storage ([ca7aec8](https://github.com/drook207/stride/commit/ca7aec8431ee88a59633594db7d226a5b3ebc96e))
- **redmine-client:** Send activity_id when creating time entries ([5705845](https://github.com/drook207/stride/commit/57058458784d0f4cb2681e915d2fdb6988f0c034))
- **ui:** Rebrand as "Stride" with a persistent sidebar and tracking bar ([cfdc1d7](https://github.com/drook207/stride/commit/cfdc1d7973cac94c7b8055f83c4f948ff8397949))
- **ui:** Add separate portrait UI for Android and iOS ([da0ac78](https://github.com/drook207/stride/commit/da0ac783f547f56d2933f142fc3f263ed29ac00d))
- **i18n:** Wire up Qt Linguist translation build and runtime loading ([59895e1](https://github.com/drook207/stride/commit/59895e1d4bbcb590b27002a8504b277d0a6a2a8b))
- **i18n:** Add English and German translation catalogs ([82f374c](https://github.com/drook207/stride/commit/82f374c041d0fc60b94d6065e1bc74fc31e8e608))
- **i18n:** Add German translations for the RedmineClient library ([6b66151](https://github.com/drook207/stride/commit/6b66151cb6fa362da149553dfcc647533360240b))
- **settings:** Add a language selection to the settings view ([22a6197](https://github.com/drook207/stride/commit/22a6197292e76c5364bdf8755424257fe11ec2f0))
- **packaging:** Add themed Qt Installer Framework installer for Windows and Linux ([02de1ed](https://github.com/drook207/stride/commit/02de1ed734778566c283587eec55093fb6b2bbc3))
- **ui:** Redesign the application icon to match the dark theme ([367001d](https://github.com/drook207/stride/commit/367001d4e2b3b09b5f1bbffbc9ea2c9014b4d55c))
- **ui:** Use the application icon as window icon ([9fdeb7c](https://github.com/drook207/stride/commit/9fdeb7c6fee2a6a93e9805a00299c15c890c6658))
- **ui:** Make the sidebar navigable with the keyboard ([0afa648](https://github.com/drook207/stride/commit/0afa64876755faad9d851838d5fd01589d61a642))
- **ui:** Unify the tracking bar layout and make it scale ([8748197](https://github.com/drook207/stride/commit/8748197802d3bc551229151e1c5cc7cde423d512))
- **ui:** Apply the tracking bar layout to the mobile tracking bar ([e7099ea](https://github.com/drook207/stride/commit/e7099ea03d82f1b9dc3859984f1aa698e4e1d708))
- **ui:** Restyle the subpages with shared layout components ([c82a9c4](https://github.com/drook207/stride/commit/c82a9c435cebfccb63ace17b897c8f1df9ce7887))
- **ui:** Add date, time range and activity inputs to Add Entry ([428912c](https://github.com/drook207/stride/commit/428912cd17516fd0d6d627ad3c0c617170994a49))
- **redmineclient:** Update a time entry's date and activity, fix id parsing ([f3e85d4](https://github.com/drook207/stride/commit/f3e85d4c790b84d11471a11688b2a38bd48d2b58))
- **ui:** Edit time entries from Recent Entries and Search Issue ([c10e46e](https://github.com/drook207/stride/commit/c10e46ea53a21afb8f4561bdc75f25d3841205ab))
- **data:** Keep the project of cached issues and recent entries ([27c4842](https://github.com/drook207/stride/commit/27c4842cbf1fcc68df6796bc00efd1fcc7e22c3a))
- **ui:** Show, group and filter by Redmine project ([755834c](https://github.com/drook207/stride/commit/755834c471c118cf709b333b820cd5c0208694de))
- **projects:** Offer only projects the user may add issues to ([671f5a2](https://github.com/drook207/stride/commit/671f5a225ca821e6ff2e5acf61fbe2c286ccb280))
- **timetracker:** Link existing local entries to their Redmine time entries ([d841914](https://github.com/drook207/stride/commit/d841914417364432b45a54f70b70f0a3f7a090ca))
- **sync:** Send time entries saved while Redmine wasn't reachable ([01e3f75](https://github.com/drook207/stride/commit/01e3f750c5b928ec6993761f984d30e5a095b2ad))
- **tracking:** Keep the running tracker across restarts ([dcf9e4b](https://github.com/drook207/stride/commit/dcf9e4b07bd99d286614f55a0adac85ad4bf582a))
- **ui:** Delete time entries ([23a8de1](https://github.com/drook207/stride/commit/23a8de13dfcc9656f4f177917b6f42c8b9fcbc10))
- **overview:** Week overview of booked time ([8862b0c](https://github.com/drook207/stride/commit/8862b0c8066d96b915778b5b8d7e81baf62e6738))
- **tracking:** Continue earlier work and pin favorite issues ([ffc72ba](https://github.com/drook207/stride/commit/ffc72baf61c67b50e7b5a01a239339593d5fa9b9))
- **desktop:** Tray, shortcuts, sleep detection and reminders ([ba24f51](https://github.com/drook207/stride/commit/ba24f5147ec9c96d4c948b656d53a9fcef7c7d5d))
- **issues:** Set the issue status when submitting time ([1a42f68](https://github.com/drook207/stride/commit/1a42f6852fdbd25735eaabc6c5a8ef4cb2f59e9e))
- **export:** Export the time entries of a period as CSV or PDF ([810b2b3](https://github.com/drook207/stride/commit/810b2b37d2bbd70887ff3dfc8cd9367776f1df57))

### Bug fixes

- **database:** Fix scalability issues and harden query handling ([30e1ddd](https://github.com/drook207/stride/commit/30e1ddd73fa119b6f701ba647d97bac020d1b490))
- **redmine-client:** Normalize base URL and enumeration endpoint paths ([1038f1f](https://github.com/drook207/stride/commit/1038f1fd65ad375d2bdc377b9154745c4921f38b))
- **qml:** Remove self-referential apiKey/baseUrl default bindings ([996133c](https://github.com/drook207/stride/commit/996133c67dd5c79547f46f8be0db75c4d494f015))
- **qml:** Fix broken issue-list refresh connection in IssueSearchInputField ([0ba5a62](https://github.com/drook207/stride/commit/0ba5a621d7627a246a62482502f1088d1994731f))
- **ui:** Remove leftover header separator line ([52f47d4](https://github.com/drook207/stride/commit/52f47d4ccd89190fbe76c7eebd49b7e68124f396))
- **ui:** Size tracking-bar fields to fit their boxes ([f6801db](https://github.com/drook207/stride/commit/f6801db7c754d851c1bfa2f620af2c95b528031b))
- **ui:** Make sidebar background transparent to match design ([282f5bc](https://github.com/drook207/stride/commit/282f5bcdd1e5d80bfd16c3ae525a84872f926b26))
- **ui:** Replace stock SpinBox styling with a plain TextField in Add Issue ([27401c5](https://github.com/drook207/stride/commit/27401c5e07c6ee99a601b5c21d019f11c143fd8f))
- **ui:** Remove redundant placeholder text that broke field styling ([9445237](https://github.com/drook207/stride/commit/9445237b12bca8023ca2ccb689bf86815ce76988))
- **ui:** Align Get button with its input field ([5565230](https://github.com/drook207/stride/commit/556523075ab2e8fa978457546f87eb5e8d35bc6c))
- **build:** Exclude GoogleTest's install rules from packaged app ([4db5713](https://github.com/drook207/stride/commit/4db57134fdcd27d7fa502a89429e71a891f1128d))
- **build:** Exclude QtKeychain's install rules from packaged app ([c041fe5](https://github.com/drook207/stride/commit/c041fe5810f08bc4814f2f2bb266d9b5bf020e25))
- **ui:** Pass the activities model to the shells instead of binding it to itself ([200924c](https://github.com/drook207/stride/commit/200924cf790d24429f1edb079eeb78a3847e4ee2))
- **i18n:** Wrap remaining hardcoded UI strings in qsTr() ([76150a4](https://github.com/drook207/stride/commit/76150a4765ed7efbdb6f3628acbc747acf91a62c))
- **ui:** Dim the window behind modal dialogs with a dark scrim ([802f451](https://github.com/drook207/stride/commit/802f4515f9d93977bfe4756630b857dea3ace2bb))
- **ui:** Submit the activity the tracking bars show ([74f71af](https://github.com/drook207/stride/commit/74f71af900104a08224fa10874beb93cdc270027))
- **redmineclient:** Stop leaking build-mode macro to consumers ([cdfe4d1](https://github.com/drook207/stride/commit/cdfe4d1014b1d2f0df507a8109edc7b2cbfde7e2))
- **timetracker:** Stop double-inserting and mis-keying new recent entries ([6b767e3](https://github.com/drook207/stride/commit/6b767e3306e27f69331bb83794fef792fca2d942))
- **ui:** Offer only bookable activities and wire up the Special field ([bc6055b](https://github.com/drook207/stride/commit/bc6055b36e84f0a09cd497810293457422d1321c))
- **timetracker:** Link and stage time entry edits per request ([a96b186](https://github.com/drook207/stride/commit/a96b1869e650c47decd17ab791e18bfd6da35d62))
- **projects:** Address review findings on projects and recent entries ([f32260b](https://github.com/drook207/stride/commit/f32260b0e1e037ccc1778e2040a4fd2b4c659df6))
- **activities:** Address review findings on activities and custom fields ([6a191d0](https://github.com/drook207/stride/commit/6a191d06e548202a0c5057fc4c40b46287cb9649))
- **docker:** Address review findings on the Redmine seeding ([59dd560](https://github.com/drook207/stride/commit/59dd560d6077020d9d0dbdf72a14bc7a534b22fd))
- Address review findings on linking and project permissions ([04f7d1a](https://github.com/drook207/stride/commit/04f7d1ad5aead0323dffd377e86b276bc653a321))
- **app:** Take the application identity from CMake ([78f33f2](https://github.com/drook207/stride/commit/78f33f2b5cf54b3da70b329bb9759e4a03a1dc7b))
- **database:** Move the old database completely or not at all ([09df950](https://github.com/drook207/stride/commit/09df950b6cee5c1806b2003fbdbe28572b33b076))
- **sync:** Link before every resend, retry while online, send edits made while sending ([cc03e61](https://github.com/drook207/stride/commit/cc03e612955587ceee9f127a76e5e0ce2d8a6ac5))
- **overview:** Debounce fetching the week, reports per purpose ([e8c9d0d](https://github.com/drook207/stride/commit/e8c9d0dd50a5de6d3533697c2347916206eda207))
- **desktop:** Keep time worked after waking, ask Redmine before reminding ([7495302](https://github.com/drook207/stride/commit/7495302e9ac7a8686cec1b1c02c2410853584cf2))
- **issues:** Set the chosen status once the time entry is on Redmine ([6ea085d](https://github.com/drook207/stride/commit/6ea085d16380039263221b64471cdf0921de1a4d))
- **export:** Own report request, fall back only on failure, documents folder ([d88d576](https://github.com/drook207/stride/commit/d88d5764540a6bda24c4ad7cbf537ffd4dccd14a))
- **issues:** Forget cached issues Redmine doesn't know ([9cc6e9f](https://github.com/drook207/stride/commit/9cc6e9f0868b50be037d0648061d6d85a7aec4c4))
- **issues:** Forget issues only on Redmine's own 404, and tidy up after ([87f681b](https://github.com/drook207/stride/commit/87f681bafcc7daee816532c5db723121a1291de0))
- **ui:** Show the loading indicator in the middle of the window ([a6fd960](https://github.com/drook207/stride/commit/a6fd9607a45aa85fe0cc3a59615b3ec7c8639c35))
- **client:** Keep loading on until the last request finished ([c2a809e](https://github.com/drook207/stride/commit/c2a809e6de99751a578b244b0d16055f31fd283e))
- **tests:** Compare the unknown API version by value ([48f167e](https://github.com/drook207/stride/commit/48f167e664c30ca79b2a1b96c51dca68122552c1))
<!-- generated by git-cliff -->
