//
//  SentPacketHistory.h
//  libraries/networking/src
//
//  Created by Yixin Wang on 6/5/2014
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SentPacketHistory_h
#define hifi_SentPacketHistory_h

#include <stdint.h>
#include <qbytearray.h>
#include <qvector.h>

class SentPacketHistory {

public:
    SentPacketHistory(int size = 1000);

    void packetSent(uint16_t sequenceNumber, const QByteArray& packet);
    const QByteArray* getPacket(uint16_t sequenceNumber) const;

private:
    QVector<QByteArray> _sentPackets;    // circular buffer
    int _newestPacketAt;
    int _numExistingPackets;

    uint16_t _newestSequenceNumber;
};

#endif
