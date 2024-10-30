//
// Created by pascalb on 28.10.24.
//

#pragma once
#include "jsonrequest.h"

#include <QObject>

namespace QRedmine::Requests::Json {

class ProjectRequest : JsonRequest {
	Q_OBJECT
public:
	explicit ProjectRequest(QObject* parent = nullptr);

	ProjectRequest(const ProjectRequest& other);
};

} // JsonRequest

// QRedmine
