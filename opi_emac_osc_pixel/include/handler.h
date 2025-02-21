/**
 * @file handler.h
 *
 */
/* Copyright (C) 2019-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HANDLER_H_
#define HANDLER_H_

#include <stdint.h>

#include "oscserver.h"

#include "ws28xxdmx.h"

class Handler: public OscServerHandler  {
public:
	Handler(WS28xxDmx *pWS28xxDmx);

	void Blackout() override {
		m_pWS28xxDmx->Blackout(true);
	}

	void Update() override {
		m_pWS28xxDmx->Blackout(false);
	}

	void Info(int32_t nHandle, uint32_t nRemoteIp, uint16_t nPortOutgoing) override;

private:
	WS28xxDmx *m_pWS28xxDmx;
	uint32_t m_nCount;
	char *m_TypeString;
};

#endif /* HANDLER_H_ */
