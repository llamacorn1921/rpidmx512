/**
 * @file spiflashinstallparams.h
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

#ifndef SPIFLASHINSTALLPARAMS_H_
#define SPIFLASHINSTALLPARAMS_H_

#include <cstdint>

struct SpiFlashInstallParamsMask {
	static constexpr auto INSTALL_UBOOT = (1U << 0);
	static constexpr auto INSTALL_UIMAGE = (1U << 1);
};

class SpiFlashInstallParams {
public:
	bool Load();
	void Dump();

	 bool GetInstalluboot() const {
		return isMaskSet(SpiFlashInstallParamsMask::INSTALL_UBOOT);
	}

	 bool GetInstalluImage() const {
		return isMaskSet(SpiFlashInstallParamsMask::INSTALL_UIMAGE);
	}

	static void staticCallbackFunction(void *p, const char *s);

private:
	void callbackFunction(const char *pLine);
	bool isMaskSet(uint32_t nMask) const {
		return (m_nSetList & nMask) == nMask;
	}

private:
	uint32_t m_nSetList { 0 };
};

#endif /* SPIFLASHINSTALLPARAMS_H_ */
