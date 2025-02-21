/**
 * @file ltcdisplayrgbset.cpp
 */
/* Copyright (C) 2020-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include "ltcdisplayrgbset.h"

#include "ltc.h"
#include "ws28xx.h"

#include "debug.h"

using namespace ltcdisplayrgb;


void LtcDisplayRgbSet::Init() {
	DEBUG_ENTRY

	DEBUG_EXIT
}

void LtcDisplayRgbSet::ShowFPS(__attribute__((unused)) ltc::Type tTimeCodeType, __attribute__((unused)) struct Colours &tColours) {
	DEBUG_ENTRY

	DEBUG_EXIT
}

void LtcDisplayRgbSet::ShowSource(__attribute__((unused)) ltc::Source tSource, __attribute__((unused)) struct Colours &tColours) {
	DEBUG_ENTRY

	DEBUG_EXIT
}

void LtcDisplayRgbSet::ShowInfo(__attribute__((unused)) const char *pInfo, __attribute__((unused)) uint32_t nLength, __attribute__((unused)) struct Colours &tColours) {
	DEBUG_ENTRY

	DEBUG_EXIT
}
