/**
 * @file main.cpp
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
#include <cassert>

#include "hardware.h"
#include "network.h"
#include "networkconst.h"
#include "storenetwork.h"
#include "ledblink.h"
#include "display.h"

#include "mdns.h"
#include "mdnsservices.h"
#if defined (ENABLE_HTTPD)
# include "httpd/httpd.h"
#endif

#include "oscclient.h"
#include "oscclientparams.h"
#include "oscclientmsgconst.h"
#include "oscclientled.h"

#include "buttonsset.h"
#include "buttonsgpio.h"
#include "buttonsmcp.h"

#include "spiflashinstall.h"
#include "spiflashstore.h"
#include "storeoscclient.h"
#include "remoteconfig.h"
#include "remoteconfigparams.h"
#include "storeremoteconfig.h"

#include "firmwareversion.h"
#include "software_version.h"

#include "displayhandler.h"

void Hardware::RebootHandler() {

}

extern "C" {

void notmain(void) {
	Hardware hw;
	Network nw;
	LedBlink lb;
	Display display;
	FirmwareVersion fw(SOFTWARE_VERSION, __DATE__, __TIME__);

	SpiFlashInstall spiFlashInstall;
	SpiFlashStore spiFlashStore;

	StoreOscClient storeOscClient;

	OscClientParams params(&storeOscClient);
	OscClient client;

	if (params.Load()) {
		params.Dump();
		params.Set(&client);
	}

	fw.Print();

	hw.SetLed(hardware::LedStatus::ON);
	lb.SetLedBlinkDisplay(new DisplayHandler);

	display.TextStatus(NetworkConst::MSG_NETWORK_INIT, Display7SegmentMessage::INFO_NETWORK_INIT, CONSOLE_YELLOW);

	StoreNetwork storeNetwork;
	nw.SetNetworkStore(&storeNetwork);
	nw.Init(&storeNetwork);
	nw.Print();

	MDNS mDns;

	mDns.Start();
	mDns.AddServiceRecord(nullptr, MDNS_SERVICE_CONFIG, 0x2905);
	mDns.AddServiceRecord(nullptr, MDNS_SERVICE_OSC, client.GetPortIncoming(), mdns::Protocol::UDP, "type=client");
#if defined (ENABLE_HTTPD)
	mDns.AddServiceRecord(nullptr, MDNS_SERVICE_HTTP, 80, mdns::Protocol::TCP, "node=OSC Client");
#endif
	mDns.Print();
#if defined (ENABLE_HTTPD)
	HttpDaemon httpDaemon;
	httpDaemon.Start();
#endif

	display.TextStatus(OscClientMsgConst::PARAMS, Display7SegmentMessage::INFO_OSCCLIENT_PARMAMS, CONSOLE_YELLOW);

	client.Print();

	ButtonsSet *pButtonsSet;

	auto *pButtonsMcp = new ButtonsMcp(&client);
	assert(pButtonsMcp != nullptr);

	if (pButtonsMcp->Start()) {
		pButtonsSet = static_cast<ButtonsSet*>(pButtonsMcp);
		client.SetLedHandler(pButtonsMcp);
	} else {
		delete pButtonsMcp;

		auto *pButtonsGpio = new ButtonsGpio(&client);
		assert(pButtonsGpio != nullptr);

		pButtonsGpio->Start();

		pButtonsSet = static_cast<ButtonsSet*>(pButtonsGpio);
		client.SetLedHandler(pButtonsGpio);
	}

	RemoteConfig remoteConfig(remoteconfig::Node::OSC_CLIENT, remoteconfig::Output::OSC, pButtonsSet->GetButtonsCount());

	StoreRemoteConfig storeRemoteConfig;
	RemoteConfigParams remoteConfigParams(&storeRemoteConfig);

	if(remoteConfigParams.Load()) {
		remoteConfigParams.Set(&remoteConfig);
		remoteConfigParams.Dump();
	}

	while (spiFlashStore.Flash())
		;

	for (uint8_t i = 1; i < 7 ; i++) {
		display.ClearLine(i);
	}

	display.Write(1, "Eth OSC Client");
	display.Printf(2, "%s.local", nw.GetHostName());
	display.Printf(3, "IP: " IPSTR " %c", IP2STR(Network::Get()->GetIp()), nw.IsDhcpKnown() ? (nw.IsDhcpUsed() ? 'D' : 'S') : ' ');
	display.Printf(4, "S : " IPSTR, IP2STR(client.GetServerIP()));
	display.Printf(5, "O : %d", client.GetPortOutgoing());
	display.Printf(6, "I : %d", client.GetPortIncoming());

	display.TextStatus(OscClientMsgConst::START, Display7SegmentMessage::INFO_OSCCLIENT_START, CONSOLE_YELLOW);

	client.Start();

	display.TextStatus(OscClientMsgConst::STARTED, Display7SegmentMessage::INFO_OSCCLIENT_STARTED, CONSOLE_GREEN);

	lb.SetMode(ledblink::Mode::NORMAL);

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		nw.Run();
		client.Run();
		pButtonsSet->Run();
		remoteConfig.Run();
		spiFlashStore.Flash();
		mDns.Run();
		lb.Run();
		display.Run();
#if defined (ENABLE_HTTPD)
		httpDaemon.Run();
#endif
	}
}
}
