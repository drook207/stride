#pragma once

#include <QEventLoop>
#include <QObject>
#include <QTimer>
#include <functional>

namespace QRedmineTesting {

/*!
 * \brief waitForSignal Runs a local QEventLoop until sender emits signal, or timeoutMs elapses.
 * Kept deliberately dependency-free (no Qt6::Test/QSignalSpy) since the test target only links
 * Qt6::{Core,Network,Quick}.
 * \return true if the signal was received before the timeout, false on timeout.
 */
template <typename Sender, typename Signal> bool waitForSignal(Sender* sender, Signal signal, int timeoutMs = 3000)
{
	QEventLoop loop;

	QTimer timeoutTimer;
	timeoutTimer.setSingleShot(true);
	QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

	QMetaObject::Connection connection = QObject::connect(sender, signal, &loop, &QEventLoop::quit);

	timeoutTimer.start(timeoutMs);
	loop.exec();

	bool signaled = timeoutTimer.isActive();
	timeoutTimer.stop();
	QObject::disconnect(connection);
	return signaled;
}

/*!
 * \brief waitUntil Pumps a local QEventLoop, polling predicate, until it returns true or
 * timeoutMs elapses. Useful for asserting the absence of a signal (e.g. errorOccurred not
 * firing), where there is nothing to directly wait for.
 * \return the final result of predicate() (i.e. whether it became true before the timeout).
 */
inline bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000)
{
	if (predicate()) {
		return true;
	}

	QEventLoop loop;

	QTimer pollTimer;
	QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
		if (predicate()) {
			loop.quit();
		}
	});
	pollTimer.start(10);

	QTimer timeoutTimer;
	timeoutTimer.setSingleShot(true);
	QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
	timeoutTimer.start(timeoutMs);

	loop.exec();

	return predicate();
}

}
