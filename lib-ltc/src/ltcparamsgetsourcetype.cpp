/**
 * @file ltcparamsgetsourcetype.cpp
 *
 */
/* Copyright (C) 2019-2022 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <cstdint>
#include <cstring>

#include "ltcparams.h"

static constexpr char s_source[static_cast<uint32_t>(ltc::Source::UNDEFINED)][9] = {
		"ltc", "artnet", "midi", "tcnet", "internal", "rtp-midi", "systime", "etc"
};

const char* LtcParams::GetSourceType(ltc::Source source) {
	if (source < ltc::Source::UNDEFINED) {
		return s_source[static_cast<uint32_t>(source)];
	}

	return "Undefined";
}

ltc::Source LtcParams::GetSourceType(const char *pType) {
	for (uint32_t i = 0; i < static_cast<uint32_t>(ltc::Source::UNDEFINED); i++) {
		if (strcasecmp(s_source[i], pType) == 0) {
			return static_cast<ltc::Source>(i);
		}
	}

	return ltc::Source::LTC;
}
