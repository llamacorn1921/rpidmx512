/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2016-2022 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
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
#include <cassert>

#include "hardware.h"
#include "network.h"
#include "ledblink.h"

#include "console.h"
#include "display.h"

#include "artnetnode.h"
#include "artnetparams.h"

// DMX output / RDM
#include "dmx.h"
#include "dmxparams.h"
#include "dmxsend.h"
#include "rdmdeviceparams.h"
#include "artnetdiscovery.h"
#ifndef H3
 // Monitor Output
# include "dmxmonitor.h"
# include "timecode.h"
#endif
// Pixel Controller
#include "pixeldmxconfiguration.h"
#include "pixeltype.h"
#include "lightset.h"
#include "pixeldmxparams.h"
#include "ws28xxdmx.h"

#if defined(ORANGE_PI)
# include "spiflashinstall.h"
# include "spiflashstore.h"
# include "storeartnet.h"
# include "storerdmdevice.h"
# include "storedmxsend.h"
# include "storepixeldmx.h"
#endif

#include "software_version.h"

using namespace artnet;

constexpr char NETWORK_INIT[] = "Network init ...";
constexpr char NODE_PARMAS[] = "Setting Node parameters ...";
constexpr char RUN_RDM[] = "Running RDM Discovery ...";
constexpr char START_NODE[] = "Starting the Node ...";
constexpr char NODE_STARTED[] = "Node started";

extern "C" {

void notmain(void) {
	Hardware hw;
	Network nw;
	LedBlink lb;
	Display display;

#if defined (ORANGE_PI)
	SpiFlashInstall spiFlashInstall;
	SpiFlashStore spiFlashStore;

	StoreDmxSend storeDmxSend;
	StorePixelDmx storePixelDmx;
	StoreRDMDevice storeRdmDevice;

	ArtNetParams artnetparams(StoreArtNet::Get());
#else
	ArtNetParams artnetparams;
#endif

	if (artnetparams.Load()) {
		artnetparams.Dump();
	}

	const auto outputType = artnetparams.GetOutputType();

	uint8_t nHwTextLength;
	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hw.GetBoardName(nHwTextLength), __DATE__, __TIME__);

	console_puts("WiFi Art-Net 3 Node ");
	console_set_fg_color(outputType == lightset::OutputType::DMX ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("DMX Output");
	console_set_fg_color(CONSOLE_WHITE);
	console_puts(" / ");
	console_set_fg_color((artnetparams.IsRdm() && (outputType == lightset::OutputType::DMX)) ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("RDM");
	console_set_fg_color(CONSOLE_WHITE);
#ifndef H3
	console_puts(" / ");
	console_set_fg_color(outputType == lightset::OutputType::MONITOR ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("Monitor");
	console_set_fg_color(CONSOLE_WHITE);
#endif
	console_puts(" / ");
	console_set_fg_color(outputType == lightset::OutputType::SPI ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("Pixel controller {4 Universes}");
	console_set_fg_color(CONSOLE_WHITE);
#ifdef H3
	console_putc('\n');
#endif
#ifndef H3
	console_set_top_row(3);
#endif

	hw.SetLed(hardware::LedStatus::ON);

	console_status(CONSOLE_YELLOW, NETWORK_INIT);
	display.TextStatus(NETWORK_INIT);

	nw.Init();

	ArtNetNode node;

#ifndef H3
	DMXMonitor monitor;
	TimeCode timecode;
#endif
	ArtNetRdmController discovery;

	console_status(CONSOLE_YELLOW, NODE_PARMAS);
	display.TextStatus(NODE_PARMAS);

	artnetparams.Set();

#ifndef H3
	if (outputType == lightset::OutputType::MONITOR) {
		timecode.Start();
		node.SetTimeCodeHandler(&timecode);
	}
#endif

	bool isSet;
	const auto nStartUniverse = artnetparams.GetUniverse(0, isSet);

	node.SetUniverseSwitch(0, lightset::PortDir::OUTPUT, nStartUniverse);

	Dmx	dmx;
	DmxSend dmxSend;
	LightSet *pSpi = nullptr;

	if (outputType == lightset::OutputType::SPI) {
		PixelDmxConfiguration pixelDmxConfiguration;

#if defined (ORANGE_PI)
		PixelDmxParams pixelDmxParams(new StorePixelDmx);
#else
		PixelDmxParams pixelDmxParams;
#endif

		if (pixelDmxParams.Load()) {
			pixelDmxParams.Dump();
			pixelDmxParams.Set(&pixelDmxConfiguration);
		}

		auto *pWS28xxDmx = new WS28xxDmx(pixelDmxConfiguration);
		assert(pWS28xxDmx != nullptr);
		pSpi = pWS28xxDmx;

		display.Printf(7, "%s:%d G%d", PixelType::GetType(pixelDmxConfiguration.GetType()), pixelDmxConfiguration.GetCount(), pixelDmxConfiguration.GetGroupingCount());

		const auto nUniverses = pWS28xxDmx->GetUniverses();

		for (uint8_t nPortIndex = 1; nPortIndex < nUniverses; nPortIndex++) {
			node.SetUniverseSwitch(nPortIndex, lightset::PortDir::OUTPUT, static_cast<uint8_t>(nStartUniverse + nPortIndex));
		}

		node.SetOutput(pSpi);
	}
#ifndef H3
	else if (outputType == lightset::OutputType::MONITOR) {
		// There is support for HEX output only
		node.SetOutput(&monitor);
		monitor.Cls();
		console_set_top_row(20);
	}
#endif
	else {
#if defined (ORANGE_PI)
		DmxParams dmxparams(&storeDmxSend);
#else
		DmxParams dmxparams;
#endif
		if (dmxparams.Load()) {
			dmxparams.Dump();
			dmxparams.Set(&dmx);
		}

		node.SetOutput(&dmxSend);

		if (artnetparams.IsRdm()) {
#if defined (ORANGE_PI)
			RDMDeviceParams rdmDeviceParams(&storeRdmDevice);
#else
			RDMDeviceParams rdmDeviceParams;
#endif
			if(rdmDeviceParams.Load()) {
				rdmDeviceParams.Set(&discovery);
				rdmDeviceParams.Dump();
			}

			discovery.Init();
			discovery.Print();

			console_status(CONSOLE_YELLOW, RUN_RDM);
			display.TextStatus(RUN_RDM);
			discovery.Full(0);

			node.SetRdmHandler((ArtNetRdm *)&discovery);
		}
	}

	node.Print();

	if (outputType == lightset::OutputType::SPI) {
		assert(pSpi != 0);
		pSpi->Print();
	} else if (outputType == lightset::OutputType::MONITOR) {
		// Nothing
	} else {
		dmxSend.Print();
	}

	for (uint8_t i = 0; i < 7; i++) {
		display.ClearLine(i);
	}

	display.Write(1, "WiFi Art-Net 3 ");

	switch (outputType) {
	case lightset::OutputType::SPI:
		display.PutString("Pixel");
		break;
	case lightset::OutputType::MONITOR:
		display.PutString("Monitor");
		break;
	default:
		if (artnetparams.IsRdm()) {
			display.PutString("RDM");
		} else {
			display.PutString("DMX");
		}
		break;
	}

	if (nw.GetOpmode() == WIFI_STA) {
		display.Printf(2, "S: %s", nw.GetSsid());
	} else {
		display.Printf(2, "AP (%s)\n", nw.IsApOpen() ? "Open" : "WPA_WPA2_PSK");
	}

	display.Printf(3, "IP: " IPSTR "", IP2STR(Network::Get()->GetIp()));

	if (nw.IsDhcpKnown()) {
		if (nw.IsDhcpUsed()) {
			display.PutString(" D");
		} else {
			display.PutString(" S");
		}
	}

	display.Printf(4, "N: " IPSTR "", IP2STR(Network::Get()->GetNetmask()));
	display.Printf(5, "U: %d", nStartUniverse);
	display.Printf(6, "Active ports: %d", node.GetActiveOutputPorts());

	console_status(CONSOLE_YELLOW, START_NODE);
	display.TextStatus(START_NODE);

	node.Start();

	console_status(CONSOLE_GREEN, NODE_STARTED);
	display.TextStatus(NODE_STARTED);

	hw.WatchdogFeed();

	for (;;) {
		hw.WatchdogFeed();
		node.Run();
		lb.Run();
#if defined (ORANGE_PI)
		spiFlashStore.Flash();
#endif
	}
}

}
