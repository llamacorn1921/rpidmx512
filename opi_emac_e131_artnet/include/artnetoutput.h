/**
 * @file artnetoutput.h
 *
 */
/* Copyright (C) 2020-2022 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef ARTNETOUTPUT_H_
#define ARTNETOUTPUT_H_

#include <cstdint>

#include "lightset.h"

#include "e131bridge.h"
#include "e131sync.h"

class ArtNetOutput: public E131Sync, public LightSet {
public:
	ArtNetOutput();

	void Handler() override;

	void Start(uint32_t nPortIndex) override;
	void Stop(uint32_t nPortIndex) override;

	void SetData(uint32_t nPortIndex, const uint8_t *pDmxData, uint32_t nLength) override;

	void Print() override {};

private:
	uint16_t m_nUniverse[e131bridge::MAX_PORTS];
};

#endif /* ARTNETOUTPUT_H_ */
