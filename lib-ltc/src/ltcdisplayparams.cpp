/**
 * @file ltcdisplayparams.cpp
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

#if !defined(__clang__)	// Needed for compiling on MacOS
# pragma GCC push_options
# pragma GCC optimize ("Os")
#endif

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>

#include "ltcdisplayparams.h"

#include "ltcdisplayparamsconst.h"
#include "devicesparamsconst.h"
// Displays
#include "ltcdisplaymax7219.h"
#include "ltcdisplayrgb.h"
#if !defined (CONFIG_LTC_DISABLE_WS28XX)
# include "pixeltype.h"
#endif

#include "readconfigfile.h"
#include "sscan.h"
#include "propertiesbuilder.h"

static constexpr char aColonBlinkMode[3][5] = { "off", "down", "up" };

using namespace ltcdisplayrgb;

namespace defaults {
static constexpr auto OLED_INTENSITY = 0x7F;
static constexpr auto MAX7219_INTENSITY = 0x04;
static constexpr auto ROTARY_FULLSTEP = 0x00;
}  // namespace defaults

LtcDisplayParams::LtcDisplayParams(LtcDisplayParamsStore *pLtcDisplayParamsStore): m_pLtcDisplayParamsStore(pLtcDisplayParamsStore) {
	m_tLtcDisplayParams.nSetList = 0;
#if !defined (CONFIG_LTC_DISABLE_WS28XX)
	m_tLtcDisplayParams.nWS28xxType = static_cast<uint8_t>(Defaults::LED_TYPE);
#endif
	m_tLtcDisplayParams.nGlobalBrightness = Defaults::GLOBAL_BRIGHTNESS;	// Not used
	m_tLtcDisplayParams.nMax7219Type = static_cast<uint8_t>(ltc::display::max7219::Types::MATRIX);
	m_tLtcDisplayParams.nMax7219Intensity = defaults::MAX7219_INTENSITY;
#if !defined (CONFIG_LTC_DISABLE_WS28XX)
	m_tLtcDisplayParams.nWS28xxRgbMapping = static_cast<uint8_t>(pixel::Map::RGB);
#endif
	m_tLtcDisplayParams.nDisplayRgbIntensity = Defaults::MASTER;
	m_tLtcDisplayParams.nDisplayRgbColonBlinkMode = static_cast<uint8_t>(Defaults::COLON_BLINK_MODE);
	m_tLtcDisplayParams.aDisplayRgbColour[static_cast<uint32_t>(ColourIndex::TIME)] = Defaults::COLOUR_TIME;
	m_tLtcDisplayParams.aDisplayRgbColour[static_cast<uint32_t>(ColourIndex::COLON)] = Defaults::COLOUR_COLON;
	m_tLtcDisplayParams.aDisplayRgbColour[static_cast<uint32_t>(ColourIndex::MESSAGE)] = Defaults::COLOUR_MESSAGE;
	m_tLtcDisplayParams.aDisplayRgbColour[static_cast<uint32_t>(ColourIndex::FPS)] = Defaults::COLOUR_FPS;
	m_tLtcDisplayParams.aDisplayRgbColour[static_cast<uint32_t>(ColourIndex::INFO)] = Defaults::COLOUR_INFO;
	m_tLtcDisplayParams.aDisplayRgbColour[static_cast<uint32_t>(ColourIndex::SOURCE)] = Defaults::COLOUR_SOURCE;
	m_tLtcDisplayParams.nWS28xxDisplayType = static_cast<uint8_t>(WS28xxType::SEGMENT);
	memset(m_tLtcDisplayParams.aInfoMessage, ' ', sizeof(m_tLtcDisplayParams.aInfoMessage));
	m_tLtcDisplayParams.nOledIntensity = defaults::OLED_INTENSITY;
	m_tLtcDisplayParams.nRotaryFullStep = defaults::ROTARY_FULLSTEP;
}

bool LtcDisplayParams::Load() {
	m_tLtcDisplayParams.nSetList = 0;

#if !defined(DISABLE_FS)
	ReadConfigFile configfile(LtcDisplayParams::staticCallbackFunction, this);

	if (configfile.Read(LtcDisplayParamsConst::FILE_NAME)) {
		// There is a configuration file
		if (m_pLtcDisplayParamsStore != nullptr) {
			m_pLtcDisplayParamsStore->Update(&m_tLtcDisplayParams);
		}
	} else
#endif
	if (m_pLtcDisplayParamsStore != nullptr) {
		m_pLtcDisplayParamsStore->Copy(&m_tLtcDisplayParams);
	} else {
		return false;
	}

	return true;
}

void LtcDisplayParams::Load(const char *pBuffer, uint32_t nLength) {
	assert(pBuffer != nullptr);
	assert(nLength != 0);
	assert(m_pLtcDisplayParamsStore != nullptr);

	if (m_pLtcDisplayParamsStore == nullptr) {
		return;
	}

	m_tLtcDisplayParams.nSetList = 0;

	ReadConfigFile config(LtcDisplayParams::staticCallbackFunction, this);

	config.Read(pBuffer, nLength);

	m_pLtcDisplayParamsStore->Update(&m_tLtcDisplayParams);
}

void LtcDisplayParams::callbackFunction(const char *pLine) {
	assert(pLine != nullptr);

	char aBuffer[16];
	uint8_t nValue8;
	uint32_t nValue32;
	uint32_t nLength = sizeof(aBuffer) - 1;

	if (Sscan::Uint8(pLine, LtcDisplayParamsConst::OLED_INTENSITY, nValue8) == Sscan::OK) {
		m_tLtcDisplayParams.nOledIntensity = nValue8;

		if (nValue8 != defaults::OLED_INTENSITY) {
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::OLED_INTENSITY;
		} else {
			m_tLtcDisplayParams.nSetList &= ~ltcdisplayparams::Mask::OLED_INTENSITY;
		}
		return;
	}

	if (Sscan::Uint8(pLine, LtcDisplayParamsConst::ROTARY_FULLSTEP, nValue8) == Sscan::OK) {
		m_tLtcDisplayParams.nRotaryFullStep = (nValue8 != 0);

		if (nValue8 != defaults::ROTARY_FULLSTEP) {
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::ROTARY_FULLSTEP;
		} else {
			m_tLtcDisplayParams.nSetList &= ~ltcdisplayparams::Mask::ROTARY_FULLSTEP;
		}
		return;
	}

	if (Sscan::Char(pLine, LtcDisplayParamsConst::MAX7219_TYPE, aBuffer, nLength) == Sscan::OK) {
		if (strncasecmp(aBuffer, "7segment", nLength) == 0) {
			m_tLtcDisplayParams.nMax7219Type = static_cast<uint8_t>(ltc::display::max7219::Types::SEGMENT);
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::MAX7219_TYPE;
		} else {
			m_tLtcDisplayParams.nMax7219Type = static_cast<uint8_t>(ltc::display::max7219::Types::MATRIX);
			m_tLtcDisplayParams.nSetList &= ~ltcdisplayparams::Mask::MAX7219_TYPE;
		}
		return;
	}

	if (Sscan::Uint8(pLine, LtcDisplayParamsConst::MAX7219_INTENSITY, nValue8) == Sscan::OK) {
		if (nValue8 <= 0x0F) {
			m_tLtcDisplayParams.nMax7219Intensity = nValue8;

			if (nValue8 != defaults::MAX7219_INTENSITY) {
				m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::MAX7219_INTENSITY;
			} else {
				m_tLtcDisplayParams.nSetList &= ~ltcdisplayparams::Mask::MAX7219_INTENSITY;
			}
		}
		return;
	}

	nLength = 8;

	if (Sscan::Char(pLine, LtcDisplayParamsConst::WS28XX_TYPE, aBuffer, nLength) == Sscan::OK) {
		aBuffer[nLength] = '\0';
		if (strncasecmp(aBuffer, "7segment", nLength) == 0) {
			m_tLtcDisplayParams.nWS28xxDisplayType = static_cast<uint8_t>(WS28xxType::SEGMENT);
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::WS28XX_DISPLAY_TYPE;
		} else if (strncasecmp(aBuffer, "matrix", nLength) == 0) {
			m_tLtcDisplayParams.nWS28xxDisplayType = static_cast<uint8_t>(WS28xxType::MATRIX);
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::WS28XX_DISPLAY_TYPE;
		}
		return;
	}

#if !defined (CONFIG_LTC_DISABLE_WS28XX)
	nLength = pixel::TYPES_MAX_NAME_LENGTH;

	if (Sscan::Char(pLine, DevicesParamsConst::TYPE, aBuffer, nLength) == Sscan::OK) {
		aBuffer[nLength] = '\0';
		const auto type = PixelType::GetType(aBuffer);

		if (type != pixel::Type::UNDEFINED) {
			m_tLtcDisplayParams.nWS28xxType = static_cast<uint8_t>(type);
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::WS28XX_TYPE;
		} else {
			m_tLtcDisplayParams.nWS28xxType = static_cast<uint8_t>(pixel::defaults::TYPE);
			m_tLtcDisplayParams.nSetList &= ~ltcdisplayparams::Mask::WS28XX_TYPE;
		}
		return;
	}

	nLength = 3;
	if (Sscan::Char(pLine, DevicesParamsConst::MAP, aBuffer, nLength) == Sscan::OK) {
		aBuffer[nLength] = '\0';
		pixel::Map tMapping;
		if ((tMapping = PixelType::GetMap(aBuffer)) != pixel::Map::UNDEFINED) {
			m_tLtcDisplayParams.nWS28xxRgbMapping = static_cast<uint8_t>(tMapping);
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::WS28XX_RGB_MAPPING;
		}
		return;
	}
#endif

	if (Sscan::Uint8(pLine, LtcDisplayParamsConst::INTENSITY, nValue8) == Sscan::OK) {
		if (nValue8 != 0) {
			m_tLtcDisplayParams.nDisplayRgbIntensity = nValue8;
			m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::DISPLAYRGB_INTENSITY;
		}
		return;
	}

	nLength = 4;
	if (Sscan::Char(pLine, LtcDisplayParamsConst::COLON_BLINK_MODE, aBuffer, nLength) == Sscan::OK) {
		aBuffer[nLength] = '\0';
		for (uint32_t i = 0; i < (sizeof(aColonBlinkMode) / sizeof(aColonBlinkMode[0])); i++) {
			if (strcasecmp(aBuffer, aColonBlinkMode[i]) == 0) {
				m_tLtcDisplayParams.nDisplayRgbColonBlinkMode = static_cast<uint8_t>(i);
				m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::DISPLAYRGB_COLON_BLINK_MODE;
				return;
			}
		}
		return;
	}

	for (uint32_t nIndex = 0; nIndex < static_cast<uint32_t>(ColourIndex::LAST); nIndex++) {
		if(Sscan::Hex24Uint32(pLine, LtcDisplayParamsConst::COLOUR[nIndex], nValue32) == Sscan::OK) {
			m_tLtcDisplayParams.aDisplayRgbColour[nIndex] = nValue32;
			m_tLtcDisplayParams.nSetList |= (ltcdisplayparams::Mask::DISLAYRGB_COLOUR_INDEX << nIndex);
			return;
		}
	}

	nLength = sizeof(m_tLtcDisplayParams.aInfoMessage);
	if (Sscan::Char(pLine, LtcDisplayParamsConst::INFO_MSG, m_tLtcDisplayParams.aInfoMessage, nLength) == Sscan::OK) {
		for (; nLength < sizeof(m_tLtcDisplayParams.aInfoMessage); nLength++) {
			m_tLtcDisplayParams.aInfoMessage[nLength] = ' ';
		}
		m_tLtcDisplayParams.nSetList |= ltcdisplayparams::Mask::INFO_MSG;
		return;
	}
}

void LtcDisplayParams::Builder(const struct ltcdisplayparams::Params *ptLtcDisplayParams, char *pBuffer, uint32_t nLength, uint32_t& nSize) {
	assert(pBuffer != nullptr);

	if (ptLtcDisplayParams != nullptr) {
		memcpy(&m_tLtcDisplayParams, ptLtcDisplayParams, sizeof(struct ltcdisplayparams::Params));
	} else {
		m_pLtcDisplayParamsStore->Copy(&m_tLtcDisplayParams);
	}

	PropertiesBuilder builder(LtcDisplayParamsConst::FILE_NAME, pBuffer, nLength);

	builder.AddComment("OLED SSD1306/11");
	builder.Add(LtcDisplayParamsConst::OLED_INTENSITY, m_tLtcDisplayParams.nOledIntensity, isMaskSet(ltcdisplayparams::Mask::OLED_INTENSITY));

	builder.AddComment("Rotary control");
	builder.Add(LtcDisplayParamsConst::ROTARY_FULLSTEP, m_tLtcDisplayParams.nRotaryFullStep, isMaskSet(ltcdisplayparams::Mask::ROTARY_FULLSTEP));

	builder.AddComment("MAX7219");
	builder.Add(LtcDisplayParamsConst::MAX7219_TYPE, m_tLtcDisplayParams.nMax7219Type == static_cast<uint8_t>(ltc::display::max7219::Types::SEGMENT) ? "7segment" : "matrix" , isMaskSet(ltcdisplayparams::Mask::MAX7219_TYPE));
	builder.Add(LtcDisplayParamsConst::MAX7219_INTENSITY, m_tLtcDisplayParams.nMax7219Intensity, isMaskSet(ltcdisplayparams::Mask::MAX7219_INTENSITY));

	builder.AddComment("RGB Display (generic)");
	builder.Add(LtcDisplayParamsConst::INTENSITY, m_tLtcDisplayParams.nDisplayRgbIntensity, isMaskSet(ltcdisplayparams::Mask::DISPLAYRGB_INTENSITY));
	builder.Add(LtcDisplayParamsConst::COLON_BLINK_MODE, m_tLtcDisplayParams.nDisplayRgbColonBlinkMode == static_cast<uint8_t>(ltcdisplayrgb::ColonBlinkMode::OFF) ? "off" : (m_tLtcDisplayParams.nDisplayRgbColonBlinkMode == static_cast<uint8_t>(ltcdisplayrgb::ColonBlinkMode::DOWN) ? "down" : "up") , isMaskSet(ltcdisplayparams::Mask::DISPLAYRGB_COLON_BLINK_MODE));

	for (uint32_t nIndex = 0; nIndex < static_cast<uint32_t>(ltcdisplayrgb::ColourIndex::LAST); nIndex++) {
		builder.AddHex24(LtcDisplayParamsConst::COLOUR[nIndex], m_tLtcDisplayParams.aDisplayRgbColour[nIndex],isMaskSet(ltcdisplayparams::Mask::DISLAYRGB_COLOUR_INDEX << nIndex));
	}

#if !defined (CONFIG_LTC_DISABLE_WS28XX)
	builder.AddComment("WS28xx (specific)");
	builder.Add(LtcDisplayParamsConst::WS28XX_TYPE, m_tLtcDisplayParams.nWS28xxDisplayType == static_cast<uint8_t>(ltcdisplayrgb::WS28xxType::SEGMENT) ? "7segment" : "matrix" , isMaskSet(ltcdisplayparams::Mask::WS28XX_DISPLAY_TYPE));
	builder.Add(DevicesParamsConst::TYPE, PixelType::GetType(static_cast<pixel::Type>(m_tLtcDisplayParams.nWS28xxType)), isMaskSet(ltcdisplayparams::Mask::WS28XX_TYPE));

	builder.AddComment("Overwrite datasheet");
	if (!isMaskSet(ltcdisplayparams::Mask::WS28XX_RGB_MAPPING)) {
		m_tLtcDisplayParams.nWS28xxRgbMapping = static_cast<uint8_t>(PixelType::GetMap(static_cast<pixel::Type>(m_tLtcDisplayParams.nWS28xxType)));
	}
	builder.Add(DevicesParamsConst::MAP, PixelType::GetMap(static_cast<pixel::Map>(m_tLtcDisplayParams.nWS28xxRgbMapping)), isMaskSet(ltcdisplayparams::Mask::WS28XX_RGB_MAPPING));
#endif

#if !defined (CONFIG_LTC_DISABLE_RGB_PANEL)
	builder.AddComment("RGB panel (specific)");
	char aTemp[sizeof(m_tLtcDisplayParams.aInfoMessage) + 1];
	memcpy(aTemp, m_tLtcDisplayParams.aInfoMessage, sizeof(m_tLtcDisplayParams.aInfoMessage));
	aTemp[sizeof(m_tLtcDisplayParams.aInfoMessage)] = '\0';
	builder.Add(LtcDisplayParamsConst::INFO_MSG, aTemp, isMaskSet(ltcdisplayparams::Mask::INFO_MSG));
#endif

	nSize = builder.GetSize();
}

void LtcDisplayParams::Save(char *pBuffer, uint32_t nLength, uint32_t& nSize) {
	if (m_pLtcDisplayParamsStore == nullptr) {
		nSize = 0;
		return;
	}

	Builder(nullptr, pBuffer, nLength, nSize);
}

void LtcDisplayParams::Set(LtcDisplayRgb *pLtcDisplayRgb) {
	assert(pLtcDisplayRgb != nullptr);

#if !defined (CONFIG_LTC_DISABLE_WS28XX)
	if (isMaskSet(ltcdisplayparams::Mask::WS28XX_RGB_MAPPING)) {
		pLtcDisplayRgb->SetMapping(static_cast<pixel::Map>(m_tLtcDisplayParams.nWS28xxRgbMapping));
	}
#endif

	if (isMaskSet(ltcdisplayparams::Mask::DISPLAYRGB_INTENSITY)) {
		pLtcDisplayRgb->SetMaster(m_tLtcDisplayParams.nDisplayRgbIntensity);
	}

	if (isMaskSet(ltcdisplayparams::Mask::DISPLAYRGB_COLON_BLINK_MODE)) {
		pLtcDisplayRgb->SetColonBlinkMode(static_cast<ColonBlinkMode>(m_tLtcDisplayParams.nDisplayRgbColonBlinkMode));
	}

	for (uint32_t nIndex = 0; nIndex < static_cast<uint32_t>(ColourIndex::LAST); nIndex++) {
		if (isMaskSet((ltcdisplayparams::Mask::DISLAYRGB_COLOUR_INDEX << nIndex))) {
			pLtcDisplayRgb->SetColour(m_tLtcDisplayParams.aDisplayRgbColour[nIndex], static_cast<ltcdisplayrgb::ColourIndex>(nIndex));
		}
	}
}

void LtcDisplayParams::staticCallbackFunction(void *p, const char *s) {
	assert(p != nullptr);
	assert(s != nullptr);

	(static_cast<LtcDisplayParams*>(p))->callbackFunction(s);
}
