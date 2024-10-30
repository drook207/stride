#include "iauthentication.h"

namespace QRedmine::Authentication {

IAuthentication::IAuthentication(QObject* parent)
	: QObject { parent }
{
}

} // namespace QRedmineClient
