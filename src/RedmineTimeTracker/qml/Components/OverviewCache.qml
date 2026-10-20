pragma Singleton
import QtQuick

/*!
 * \brief Remembers the last report fetched for the overview per week, beyond the view's lifetime.
 *
 * OverviewView is created anew on each navigation; without this it would ask Redmine for the same
 * week every time. An answer is reused for maxAge milliseconds.
 */
QtObject {
    //! How long a fetched report counts as current.
    readonly property int maxAge: 60 * 1000
    //! "from|to" -> {time, entries}.
    property var reports: ({})

    //! The entries fetched for the range less than maxAge ago, or null.
    function lookup(from, to) {
        const report = reports[from + "|" + to];
        if (report && Date.now() - report.time < maxAge) {
            return report.entries;
        }
        return null;
    }

    function store(from, to, entries) {
        reports[from + "|" + to] = {time: Date.now(), entries: entries};
    }

    //! Forgets everything, e.g. when local entries changed so that a report is outdated.
    function clear() {
        reports = ({});
    }
}
