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

#ifndef TIMER_H
#define TIMER_H

#include "header.h"
#include "callback.h"

using namespace std;

class CTimer : public CBasisThread
{
private:
    int mInstances;

    CCallback *mCallback;

    uint64_t pid;

    bool unreachableSignaled;
    bool streamsDroppedSignaled;
    bool forbiddenSignaled;
    bool tlsParametersMissingSignaled;
    unsigned long long mInitialCallbackDelay;

public:
    CTimer();

    virtual ~CTimer();

    CTimer(int nInstances, CCallback *pCallback, unsigned long long nInitialCallbackDelay);

    int run();
};

#endif
