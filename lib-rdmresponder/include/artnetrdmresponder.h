/**
 * @file artnetrdmresponder.h
 *
 */
/* Copyright (C) 2018-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef ARTNETRDMRESPONDER_H_
#define ARTNETRDMRESPONDER_H_

#include <cstdint>

#include "artnetrdm.h"

#include "rdmdeviceresponder.h"
#include "rdmpersonality.h"
#include "rdmhandler.h"
#include "rdm.h"

#include "lightset.h"

#if defined (NODE_RDMNET_LLRP_ONLY)
# error "Cannot be both RDMNet Device and RDM Responder"
#endif

class ArtNetRdmResponder: public ArtNetRdm, public RDMDeviceResponder, RDMHandler {
public:
	ArtNetRdmResponder(RDMPersonality **pRDMPersonalities, uint32_t nPersonalityCount);
	~ArtNetRdmResponder() override;

	void Full(uint32_t nPortIndex) override;
	uint32_t GetUidCount(uint32_t nPortIndex) override;
	void Copy(uint32_t nPortIndex, uint8_t *) override;
	const uint8_t *Handler(uint32_t nPortIndex, const uint8_t *) override;

	uint16_t GetDmxStartAddress(uint16_t nSubDevice = RDM_ROOT_DEVICE) {
		return RDMDeviceResponder::GetDmxStartAddress(nSubDevice);
	}

	uint16_t GetDmxFootPrint(uint16_t nSubDevice = RDM_ROOT_DEVICE) {
		return RDMDeviceResponder::GetDmxFootPrint(nSubDevice);
	}

	static ArtNetRdmResponder* Get() {
		return s_pThis;
	}

private:
	static ArtNetRdmResponder *s_pThis;
	static TRdmMessage s_RdmCommand;
};

#endif /* ARTNETRDMRESPONDER_H_ */
