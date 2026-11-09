// Platform-neutral network interface discovery is shared with the service.
#include "utils.h"
#include "commonutils.h"

DevConnSDK::Utils::Utils() {}
QString DevConnSDK::Utils::getLocalIPv4Address()
{
    return CommonUtils::getLocalIPv4Address();
}
QStringList DevConnSDK::Utils::getLocalIPv4AddressList()
{
    return CommonUtils::getLocalIPv4AddressList();
}
