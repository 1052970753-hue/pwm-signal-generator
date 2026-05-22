#ifndef SIM_PROTOCOL_H
#define SIM_PROTOCOL_H

#include "../common/protocol_defs.h"
#include <QByteArray>

class Protocol {
public:
    static StatusData parseStatusResponse(const QByteArray &data);
    static QByteArray encodeWritePwm(const PwmWriteReq &req);
    static QByteArray encodeWriteFgDiv(const FgDivReq &req);
};

#endif
