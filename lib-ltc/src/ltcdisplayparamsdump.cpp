/**
 * @file ltcdisplayparamsdump.cpp
 *
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

#if !defined(__clang__)	// Needed for compiling on MacOS
# pragma GCC push_options
# pragma GCC optimize ("Os")
#endif

#include <cstdint>
#include <cstdio>

#include "ltcdisplayparams.h"
#include "ltcdisplayparamsconst.h"

#include "devicesparamsconst.h"

#if !defined (CONFIG_LTC_DISABLE_WS28XX)
# include "pixeltype.h"
#endif

void LtcDisplayParams::Dump() {
#ifndef NDEBUG
	printf("%s::%s \'%s\':\n", __FILE__, __FUNCTION__, LtcDisplayParamsConst::FILE_NAME);

	if (isMaskSet(ltcdisplayparams::Mask::WS28XX_DISPLAY_TYPE)) {
		printf(" %s=%d [%s]\n", LtcDisplayParamsConst::WS28XX_TYPE, m_tLtcDisplayParams.nWS28xxDisplayType, m_tLtcDisplayParams.nWS28xxDisplayType == static_cast<uint8_t>(ltcdisplayrgb::WS28xxType::SEGMENT) ? "7segment" : "matrix");
	}

#if !defined (CONFIG_LTC_DISABLE_WS28XX)
	if (isMaskSet(ltcdisplayparams::Mask::WS28XX_TYPE)) {
		printf(" %s=%s [%d]\n", DevicesParamsConst::TYPE, PixelType::GetType(static_cast<pixel::Type>(m_tLtcDisplayParams.nWS28xxType)), static_cast<int>(m_tLtcDisplayParams.nWS28xxType));
	}

	if (isMaskSet(ltcdisplayparams::Mask::WS28XX_RGB_MAPPING)) {
		printf(" %s=%s [%d]\n", DevicesParamsConst::MAP, PixelType::GetMap(static_cast<pixel::Map>(m_tLtcDisplayParams.nWS28xxRgbMapping)), static_cast<int>(m_tLtcDisplayParams.nWS28xxRgbMapping));
	}
#endif

	if (isMaskSet(ltcdisplayparams::Mask::DISPLAYRGB_INTENSITY)) {
		printf(" %s=%d\n", LtcDisplayParamsConst::INTENSITY, m_tLtcDisplayParams.nDisplayRgbIntensity);
	}

	if (isMaskSet(ltcdisplayparams::Mask::DISPLAYRGB_COLON_BLINK_MODE)) {
		printf(" %s=%d\n", LtcDisplayParamsConst::COLON_BLINK_MODE, m_tLtcDisplayParams.nDisplayRgbColonBlinkMode);
	}

	for (uint32_t nIndex = 0; nIndex < static_cast<uint32_t>(ltcdisplayrgb::ColourIndex::LAST); nIndex++) {
		if (isMaskSet((ltcdisplayparams::Mask::DISLAYRGB_COLOUR_INDEX << nIndex))) {
			printf(" %s=%.6x\n", LtcDisplayParamsConst::COLOUR[nIndex], m_tLtcDisplayParams.aDisplayRgbColour[nIndex]);
		}
	}

	if (isMaskSet(ltcdisplayparams::Mask::GLOBAL_BRIGHTNESS)) {
		printf(" %s=%d\n", DevicesParamsConst::GLOBAL_BRIGHTNESS, static_cast<int>(m_tLtcDisplayParams.nGlobalBrightness));
	}

	if (isMaskSet(ltcdisplayparams::Mask::MAX7219_TYPE)) {
		printf(" %s=%d [%s]\n", LtcDisplayParamsConst::MAX7219_TYPE, m_tLtcDisplayParams.nMax7219Type, m_tLtcDisplayParams.nMax7219Type == static_cast<uint8_t>(ltc::display::max7219::Types::SEGMENT) ? "7segment" : "matrix");
	}

	if (isMaskSet(ltcdisplayparams::Mask::MAX7219_INTENSITY)) {
		printf(" %s=%d\n", LtcDisplayParamsConst::MAX7219_INTENSITY, m_tLtcDisplayParams.nMax7219Intensity);
	}

	if (isMaskSet(ltcdisplayparams::Mask::OLED_INTENSITY)) {
		printf(" %s=%d\n", LtcDisplayParamsConst::OLED_INTENSITY, m_tLtcDisplayParams.nOledIntensity);
	}

	if (isMaskSet(ltcdisplayparams::Mask::ROTARY_FULLSTEP)) {
		printf(" %s=%d\n", LtcDisplayParamsConst::ROTARY_FULLSTEP, m_tLtcDisplayParams.nRotaryFullStep);
	}
#endif
}
