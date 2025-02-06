/*
    Copyright (C) 2016-2025 zafaco GmbH

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "upload_sender.h"

CUploadSender::CUploadSender()
{
}

CUploadSender::~CUploadSender()
{
}

CUploadSender::CUploadSender(CConnection *nConnection)
{
    mConnection = nConnection;
}

int CUploadSender::run()
{
    CTool::getPid(pid);

    TRC_INFO(("Starting Sender Thread with PID: " + CTool::toString(pid)).c_str());

    const char *firstChar = randomDataValues.data();

    unsigned long long nPointer = 1;
    int mResponse;

    while (RUNNING && !::hasError)
    {
        if (nPointer + MAX_PACKET_SIZE > randomDataValues.size())
        {
            nPointer += MAX_PACKET_SIZE;
            nPointer -= randomDataValues.size();
        }

        mResponse = mConnection->send(firstChar + nPointer, MAX_PACKET_SIZE, 0);

        nPointer += MAX_PACKET_SIZE;

        if (mResponse == -1 || mResponse == 0)
        {
            TRC_ERR("Received an Error: Upload SEND == " + std::to_string(mResponse));

            break;
        }

        if (m_fStop)
            break;
    }

    TRC_DEBUG(("Ending Sender Thread with PID: " + CTool::toString(pid)).c_str());

    return 0;
}
