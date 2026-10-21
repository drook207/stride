#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSortFilterProxyModel>

namespace Stride {

class QmlExposedSortFilterModelProxy : public QSortFilterProxyModel {
	Q_OBJECT
	QML_ELEMENT

public:
	explicit QmlExposedSortFilterModelProxy(QObject* parent = nullptr);
};
}
