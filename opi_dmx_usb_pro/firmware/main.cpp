/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2019-2020 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <stdio.h>
#include <stdint.h>

#include "hardware.h"
#include "networkbaremetalmacaddress.h"
#include "ledblink.h"

#include "display.h"

#include "widget.h"
#include "widgetparams.h"
#include "h3/widgetstore.h"
#include "rdmdeviceparams.h"

#include "spiflashinstall.h"
#include "spiflashstore.h"

#include "storewidget.h"
#include "storerdmdevice.h"

#include "software_version.h"

#ifndef ALIGNED
# define ALIGNED __attribute__ ((aligned (4)))
#endif

using namespace dmxsingle;
using namespace dmx;

static char widget_mode_names[4][12] ALIGNED = {"DMX_RDM", "DMX", "RDM" , "RDM_SNIFFER" };
static const struct TRDMDeviceInfoData deviceLabel ALIGNED = { const_cast<char*>("Orange Pi Zero DMX USB Pro"), 26 };

extern "C" {

void notmain(void) {
	Hardware hw;
	NetworkBaremetalMacAddress nw;
	LedBlink lb;
	Display display(DisplayType::UNKNOWN); 	// Display is not supported. We just need a pointer to object

	SpiFlashInstall spiFlashInstall;
	SpiFlashStore spiFlashStore;

	StoreWidget storeWidget;
	StoreRDMDevice storeRDMDevice;

	Widget widget;
	widget.SetPortDirection(0, PortDirection::INP, false);

	WidgetParams widgetParams(&storeWidget);

	if (widgetParams.Load()) {
		widgetParams.Set();
		widgetParams.Dump();
	}

	RDMDeviceParams rdmDeviceParams(&storeRDMDevice);

	widget.SetLabel(&deviceLabel);

	if (rdmDeviceParams.Load()) {
		rdmDeviceParams.Set(&widget);
		rdmDeviceParams.Dump();
	}

	widget.Init();

	const auto *pRdmDeviceUid = widget.GetUID();
	struct TRDMDeviceInfoData tRdmDeviceLabel;
	widget.GetLabel(&tRdmDeviceLabel);
	const auto tWidgetMode = widgetParams.GetMode();

	uint8_t nHwTextLength;
	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hw.GetBoardName(nHwTextLength), __DATE__, __TIME__);
	printf("RDM Controller with USB [Compatible with Enttec USB Pro protocol], Widget mode : %d (%s)\n", tWidgetMode, widget_mode_names[static_cast<uint32_t>(tWidgetMode)]);
	printf("Device UUID : %.2x%.2x:%.2x%.2x%.2x%.2x, ", pRdmDeviceUid[0], pRdmDeviceUid[1], pRdmDeviceUid[2], pRdmDeviceUid[3], pRdmDeviceUid[4], pRdmDeviceUid[5]);
	printf("Label : %.*s\n", static_cast<int>(tRdmDeviceLabel.length), reinterpret_cast<const char*>(tRdmDeviceLabel.data));

	hw.WatchdogInit();

	if (tWidgetMode == widget::Mode::RDM_SNIFFER) {
		widget.SetPortDirection(0, PortDirection::INP, true);
		widget.SnifferFillTransmitBuffer();	// Prevent missing first frame
	}

	for (;;) {
		hw.WatchdogFeed();
		widget.Run();
		lb.Run();
		spiFlashStore.Flash();
	}
}

}
