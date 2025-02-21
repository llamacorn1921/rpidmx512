/**
 * @file e131bridge.cpp
 *
 */
/* Copyright (C) 2016-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>

#include "e131bridge.h"

#include "e117const.h"

#include "lightset.h"
#include "lightsetdata.h"

#include "hardware.h"
#include "network.h"
#include "ledblink.h"

#include "debug.h"

using namespace e131;
using namespace e131bridge;

E131Bridge *E131Bridge::s_pThis = nullptr;

E131Bridge::E131Bridge() {
	assert(Hardware::Get() != nullptr);
	assert(Network::Get() != nullptr);
	assert(LedBlink::Get() != nullptr);

	assert(s_pThis == nullptr);
	s_pThis = this;

	for (uint32_t i = 0; i < e131bridge::MAX_PORTS; i++) {
		memset(&m_OutputPort[i], 0, sizeof(OutputPort));
		memset(&m_InputPort[i], 0, sizeof(InputPort));
		m_InputPort[i].nPriority = 100;
	}

	memset(&m_State, 0, sizeof(State));
	m_State.nPriority = priority::LOWEST;

	char aSourceName[e131::SOURCE_NAME_LENGTH];
	uint8_t nLength;
	snprintf(aSourceName, e131::SOURCE_NAME_LENGTH, "%.48s %s", Network::Get()->GetHostName(), Hardware::Get()->GetBoardName(nLength));
	SetSourceName(aSourceName);

	m_nHandle = Network::Get()->Begin(e131::UDP_PORT); 	// This must be here (and not in Start)
	assert(m_nHandle != -1);							// ToDO Rewrite SetUniverse

	Hardware::Get()->GetUuid(m_Cid);
}

E131Bridge::~E131Bridge() {
	DEBUG_ENTRY

	DEBUG_EXIT
}

void E131Bridge::Start() {
	if (m_pE131DmxIn != nullptr) {
		if (m_pE131DataPacket == nullptr) {
			struct in_addr addr;
			static_cast<void>(inet_aton("239.255.0.0", &addr));
			m_DiscoveryIpAddress = addr.s_addr | ((universe::DISCOVERY & static_cast<uint32_t>(0xFF)) << 24) | ((universe::DISCOVERY & 0xFF00) << 8);
			// TE131DataPacket
			m_pE131DataPacket = new TE131DataPacket;
			assert(m_pE131DataPacket != nullptr);
			FillDataPacket();
			// TE131DiscoveryPacket
			m_pE131DiscoveryPacket = new TE131DiscoveryPacket;
			assert(m_pE131DiscoveryPacket != nullptr);
			FillDiscoveryPacket();
		}

		for (uint32_t nPortIndex = 0; nPortIndex < e131bridge::MAX_PORTS; nPortIndex++) {
			if (m_InputPort[nPortIndex].genericPort.bIsEnabled) {
				m_pE131DmxIn->Start(nPortIndex);
			}
		}
	}

	LedBlink::Get()->SetMode(ledblink::Mode::NORMAL);
}

void E131Bridge::Stop() {
	m_State.IsNetworkDataLoss = true;

	for (uint32_t nPortIndex = 0; nPortIndex < e131bridge::MAX_PORTS; nPortIndex++) {
		if (m_pLightSet != nullptr) {
			m_pLightSet->Stop(nPortIndex);
		}
		lightset::Data::ClearLength(nPortIndex);
		m_OutputPort[nPortIndex].IsDataPending = false;
	}

	if (m_pE131DmxIn != nullptr) {
		for (uint32_t nPortIndex = 0; nPortIndex < e131bridge::MAX_PORTS; nPortIndex++) {
			if (m_InputPort[nPortIndex].genericPort.bIsEnabled) {
				m_pE131DmxIn->Stop(nPortIndex);
			}
		}
	}

	LedBlink::Get()->SetMode(ledblink::Mode::OFF_OFF);
}

void E131Bridge::SetSynchronizationAddress(bool bSourceA, bool bSourceB, uint16_t nSynchronizationAddress) {
	DEBUG_ENTRY
	DEBUG_PRINTF("bSourceA=%d, bSourceB=%d, nSynchronizationAddress=%d", bSourceA, bSourceB, nSynchronizationAddress);

	assert(nSynchronizationAddress != 0);

	uint16_t *pSynchronizationAddressSource;

	if (bSourceA) {
		pSynchronizationAddressSource = &m_State.nSynchronizationAddressSourceA;
	} else if (bSourceB) {
		pSynchronizationAddressSource = &m_State.nSynchronizationAddressSourceB;
	} else {
		DEBUG_EXIT
		return; // Just make the compiler happy
	}

	if (*pSynchronizationAddressSource == 0) {
		*pSynchronizationAddressSource = nSynchronizationAddress;
		DEBUG_PUTS("SynchronizationAddressSource == 0");
	} else if (*pSynchronizationAddressSource != nSynchronizationAddress) {
		// e131bridge::MAX_PORTS forces to check all ports
		LeaveUniverse(e131bridge::MAX_PORTS, *pSynchronizationAddressSource);
		*pSynchronizationAddressSource = nSynchronizationAddress;
		DEBUG_PUTS("SynchronizationAddressSource != nSynchronizationAddress");
	} else {
		DEBUG_PUTS("Already received SynchronizationAddress");
		DEBUG_EXIT
		return;
	}

	Network::Get()->JoinGroup(m_nHandle, universe_to_multicast_ip(nSynchronizationAddress));

	DEBUG_EXIT
}

void E131Bridge::LeaveUniverse(uint32_t nPortIndex, uint16_t nUniverse) {
	DEBUG_ENTRY
	DEBUG_PRINTF("nPortIndex=%d, nUniverse=%d", nPortIndex, nUniverse);

	for (uint32_t i = 0; i < e131bridge::MAX_PORTS; i++) {
		DEBUG_PRINTF("\tnm_OutputPort[%d].nUniverse=%d", i, m_OutputPort[i].genericPort.nUniverse);

		if (i == nPortIndex) {
			continue;
		}
		if (m_OutputPort[i].genericPort.nUniverse == nUniverse) {
			DEBUG_EXIT
			return;
		}
	}

	Network::Get()->LeaveGroup(m_nHandle, universe_to_multicast_ip(nUniverse));

	DEBUG_EXIT
}

void E131Bridge::SetUniverse(uint32_t nPortIndex, lightset::PortDir dir, uint16_t nUniverse) {
	assert(nPortIndex < e131bridge::MAX_PORTS);
	assert(dir <= lightset::PortDir::DISABLE);
	assert((nUniverse >= universe::DEFAULT) && (nUniverse <=universe::MAX));

	if ((dir == lightset::PortDir::INPUT) && (nPortIndex < e131bridge::MAX_PORTS)) {
		if (m_InputPort[nPortIndex].genericPort.bIsEnabled) {
			if (m_InputPort[nPortIndex].genericPort.nUniverse == nUniverse) {
				return;
			}
		} else {
			m_State.nActiveInputPorts = static_cast<uint8_t>(m_State.nActiveInputPorts + 1);
			assert(m_State.nActiveInputPorts <= e131bridge::MAX_PORTS);
			m_InputPort[nPortIndex].genericPort.bIsEnabled = true;
		}

		m_InputPort[nPortIndex].genericPort.nUniverse = nUniverse;
		m_InputPort[nPortIndex].nMulticastIp = universe_to_multicast_ip(nUniverse);

		return;
	}

	if (dir == lightset::PortDir::DISABLE) {
		if (nPortIndex < e131bridge::MAX_PORTS) {
			if (m_OutputPort[nPortIndex].genericPort.bIsEnabled) {
				m_OutputPort[nPortIndex].genericPort.bIsEnabled = false;
				m_State.nActiveOutputPorts = static_cast<uint8_t>(m_State.nActiveOutputPorts - 1);
				LeaveUniverse(nPortIndex, nUniverse);
			}
		}

		if (nPortIndex < e131bridge::MAX_PORTS) {
			if (m_InputPort[nPortIndex].genericPort.bIsEnabled) {
				m_InputPort[nPortIndex].genericPort.bIsEnabled = false;
				m_State.nActiveInputPorts = static_cast<uint8_t>(m_State.nActiveInputPorts - 1);
			}
		}

		return;
	}

	// From here we handle Output ports only

	if (m_OutputPort[nPortIndex].genericPort.bIsEnabled) {
		if (m_OutputPort[nPortIndex].genericPort.nUniverse == nUniverse) {
			return;
		} else {
			LeaveUniverse(nPortIndex, nUniverse);
		}
	} else {
		m_State.nActiveOutputPorts = static_cast<uint8_t>(m_State.nActiveOutputPorts + 1);
		assert(m_State.nActiveOutputPorts <= e131bridge::MAX_PORTS);
		m_OutputPort[nPortIndex].genericPort.bIsEnabled = true;
	}

	Network::Get()->JoinGroup(m_nHandle, universe_to_multicast_ip(nUniverse));

	m_OutputPort[nPortIndex].genericPort.nUniverse = nUniverse;
}

bool E131Bridge::GetUniverse(uint32_t nPortIndex, uint16_t &nUniverse, lightset::PortDir portDir) const {
	if (portDir == lightset::PortDir::INPUT) {
		if (nPortIndex < e131bridge::MAX_PORTS) {
			nUniverse = m_InputPort[nPortIndex].genericPort.nUniverse;

			return m_InputPort[nPortIndex].genericPort.bIsEnabled;
		}

		return false;
	}

	assert(nPortIndex < e131bridge::MAX_PORTS);

	nUniverse = m_OutputPort[nPortIndex].genericPort.nUniverse;

	return m_OutputPort[nPortIndex].genericPort.bIsEnabled;
}

void E131Bridge::UpdateMergeStatus(const uint32_t nPortIndex) {
	if (!m_State.IsMergeMode) {
		m_State.IsMergeMode = true;
		m_State.IsChanged = true;
	}

	m_OutputPort[nPortIndex].IsMerging = true;
}

void E131Bridge::CheckMergeTimeouts(uint32_t nPortIndex) {
	assert(nPortIndex < e131bridge::MAX_PORTS);

	const auto timeOutA = m_nCurrentPacketMillis - m_OutputPort[nPortIndex].sourceA.nMillis;

	if (timeOutA > (MERGE_TIMEOUT_SECONDS * 1000U)) {
		m_OutputPort[nPortIndex].sourceA.nIp = 0;
		memset(m_OutputPort[nPortIndex].sourceA.cid, 0, e131::CID_LENGTH);
		m_OutputPort[nPortIndex].IsMerging = false;
	}

	const auto timeOutB = m_nCurrentPacketMillis - m_OutputPort[nPortIndex].sourceB.nMillis;

	if (timeOutB > (MERGE_TIMEOUT_SECONDS * 1000U)) {
		m_OutputPort[nPortIndex].sourceB.nIp = 0;
		memset(m_OutputPort[nPortIndex].sourceB.cid, 0, e131::CID_LENGTH);
		m_OutputPort[nPortIndex].IsMerging = false;
	}

	auto bIsMerging = false;

	for (uint32_t i = 0; i < e131bridge::MAX_PORTS; i++) {
		bIsMerging |= m_OutputPort[i].IsMerging;
	}

	if (!bIsMerging) {
		m_State.IsChanged = true;
		m_State.IsMergeMode = false;
	}
}

bool E131Bridge::IsPriorityTimeOut(uint32_t nPortIndex) const {
	assert(nPortIndex < e131bridge::MAX_PORTS);

	const auto timeOutA = m_nCurrentPacketMillis - m_OutputPort[nPortIndex].sourceA.nMillis;
	const auto timeOutB = m_nCurrentPacketMillis - m_OutputPort[nPortIndex].sourceB.nMillis;

	if ( (m_OutputPort[nPortIndex].sourceA.nIp != 0) && (m_OutputPort[nPortIndex].sourceB.nIp != 0) ) {
		if ( (timeOutA < (PRIORITY_TIMEOUT_SECONDS * 1000U)) || (timeOutB < (PRIORITY_TIMEOUT_SECONDS * 1000U)) ) {
			return false;
		} else {
			return true;
		}
	} else if ( (m_OutputPort[nPortIndex].sourceA.nIp != 0) && (m_OutputPort[nPortIndex].sourceB.nIp == 0) ) {
		if (timeOutA > (PRIORITY_TIMEOUT_SECONDS * 1000U)) {
			return true;
		}
	} else if ( (m_OutputPort[nPortIndex].sourceA.nIp == 0) && (m_OutputPort[nPortIndex].sourceB.nIp != 0) ) {
		if (timeOutB > (PRIORITY_TIMEOUT_SECONDS * 1000U)) {
			return true;
		}
	}

	return false;
}

bool E131Bridge::isIpCidMatch(const struct Source *source) const {
	if (source->nIp != m_E131.IPAddressFrom) {
		return false;
	}

	if (memcmp(source->cid, m_E131.E131Packet.Raw.RootLayer.Cid, e131::CID_LENGTH) != 0) {
		return false;
	}

	return true;
}

void E131Bridge::HandleDmx() {
	const auto *pDmxData = &m_E131.E131Packet.Data.DMPLayer.PropertyValues[1];
	const auto nDmxSlots = __builtin_bswap16(m_E131.E131Packet.Data.DMPLayer.PropertyValueCount) - 1U;

	for (uint32_t nPortIndex = 0; nPortIndex < e131bridge::MAX_PORTS; nPortIndex++) {
		if (!m_OutputPort[nPortIndex].genericPort.bIsEnabled) {
			continue;
		}

		// Frame layer
		// 8.2 Association of Multicast Addresses and Universe
		// Note: The identity of the universe shall be determined by the universe number in the
		// packet and not assumed from the multicast address.
		if (m_E131.E131Packet.Data.FrameLayer.Universe != __builtin_bswap16(m_OutputPort[nPortIndex].genericPort.nUniverse)) {
			continue;
		}

		auto *pSourceA = &m_OutputPort[nPortIndex].sourceA;
		auto *pSourceB = &m_OutputPort[nPortIndex].sourceB;

		const auto ipA = pSourceA->nIp;
		const auto ipB = pSourceB->nIp;

		const auto isSourceA = isIpCidMatch(pSourceA);
		const auto isSourceB = isIpCidMatch(pSourceB);

		// 6.9.2 Sequence Numbering
		// Having first received a packet with sequence number A, a second packet with sequence number B
		// arrives. If, using signed 8-bit binary arithmetic, B – A is less than or equal to 0, but greater than -20 then
		// the packet containing sequence number B shall be deemed out of sequence and discarded
		if (isSourceA) {
			const auto diff = static_cast<int8_t>(m_E131.E131Packet.Data.FrameLayer.SequenceNumber - pSourceA->nSequenceNumberData);
			pSourceA->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			if ((diff <= 0) && (diff > -20)) {
				continue;
			}
		} else if (isSourceB) {
			const auto diff = static_cast<int8_t>(m_E131.E131Packet.Data.FrameLayer.SequenceNumber - pSourceB->nSequenceNumberData);
			pSourceB->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			if ((diff <= 0) && (diff > -20)) {
				continue;
			}
		}

		// This bit, when set to 1, indicates that the data in this packet is intended for use in visualization or media
		// server preview applications and shall not be used to generate live output.
		if ((m_E131.E131Packet.Data.FrameLayer.Options & OptionsMask::PREVIEW_DATA) != 0) {
			continue;
		}

		// Upon receipt of a packet containing this bit set to a value of 1, receiver shall enter network data loss condition.
		// Any property values in these packets shall be ignored.
		if ((m_E131.E131Packet.Data.FrameLayer.Options & OptionsMask::STREAM_TERMINATED) != 0) {
			if (isSourceA || isSourceB) {
				SetNetworkDataLossCondition(isSourceA, isSourceB);
			}
			continue;
		}

		if (m_State.IsMergeMode) {
			if (__builtin_expect((!m_State.bDisableMergeTimeout), 1)) {
				CheckMergeTimeouts(nPortIndex);
			}
		}

		if (m_E131.E131Packet.Data.FrameLayer.Priority < m_State.nPriority ){
			if (!IsPriorityTimeOut(nPortIndex)) {
				continue;
			}
			m_State.nPriority = m_E131.E131Packet.Data.FrameLayer.Priority;
		} else if (m_E131.E131Packet.Data.FrameLayer.Priority > m_State.nPriority) {
			m_OutputPort[nPortIndex].sourceA.nIp = 0;
			m_OutputPort[nPortIndex].sourceB.nIp = 0;
			m_State.IsMergeMode = false;
			m_State.nPriority = m_E131.E131Packet.Data.FrameLayer.Priority;
		}

		if ((ipA == 0) && (ipB == 0)) {
			//printf("1. First package from Source\n");
			pSourceA->nIp = m_E131.IPAddressFrom;
			pSourceA->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			memcpy(pSourceA->cid, m_E131.E131Packet.Data.RootLayer.Cid, 16);
			pSourceA->nMillis = m_nCurrentPacketMillis;
			lightset::Data::SetSourceA(nPortIndex, pDmxData, nDmxSlots);
		} else if (isSourceA && (ipB == 0)) {
			//printf("2. Continue package from SourceA\n");
			pSourceA->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			pSourceA->nMillis = m_nCurrentPacketMillis;
			lightset::Data::SetSourceA(nPortIndex, pDmxData, nDmxSlots);
		} else if ((ipA == 0) && isSourceB) {
			//printf("3. Continue package from SourceB\n");
			pSourceB->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			pSourceB->nMillis = m_nCurrentPacketMillis;
			lightset::Data::SetSourceB(nPortIndex, pDmxData, nDmxSlots);
		} else if (!isSourceA && (ipB == 0)) {
			//printf("4. New ip, start merging\n");
			pSourceB->nIp = m_E131.IPAddressFrom;
			pSourceB->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			memcpy(pSourceB->cid, m_E131.E131Packet.Data.RootLayer.Cid, 16);
			pSourceB->nMillis = m_nCurrentPacketMillis;
			UpdateMergeStatus(nPortIndex);
			lightset::Data::MergeSourceB(nPortIndex, pDmxData, nDmxSlots, m_OutputPort[nPortIndex].mergeMode);
		} else if ((ipA == 0) && !isSourceB) {
			//printf("5. New ip, start merging\n");
			pSourceA->nIp = m_E131.IPAddressFrom;
			pSourceA->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			memcpy(pSourceA->cid, m_E131.E131Packet.Data.RootLayer.Cid, 16);
			pSourceA->nMillis = m_nCurrentPacketMillis;
			UpdateMergeStatus(nPortIndex);
			lightset::Data::MergeSourceA(nPortIndex, pDmxData, nDmxSlots, m_OutputPort[nPortIndex].mergeMode);
		} else if (isSourceA && !isSourceB) {
			//printf("6. Continue merging\n");
			pSourceA->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			pSourceA->nMillis = m_nCurrentPacketMillis;
			UpdateMergeStatus(nPortIndex);
			lightset::Data::MergeSourceA(nPortIndex, pDmxData, nDmxSlots, m_OutputPort[nPortIndex].mergeMode);
		} else if (!isSourceA && isSourceB) {
			//printf("7. Continue merging\n");
			pSourceB->nSequenceNumberData = m_E131.E131Packet.Data.FrameLayer.SequenceNumber;
			pSourceB->nMillis = m_nCurrentPacketMillis;
			UpdateMergeStatus(nPortIndex);
			lightset::Data::MergeSourceB(nPortIndex, pDmxData, nDmxSlots, m_OutputPort[nPortIndex].mergeMode);
		}
#ifndef NDEBUG
		else if (isSourceA && isSourceB) {
			printf("8. Source matches both buffers, this shouldn't be happening!\n");
			assert(0);
			return;
		} else if (!isSourceA && !isSourceB) {
			printf("9. More than two sources, discarding data\n");
			assert(0);
			return;
		}
#endif
		else {
			printf("0. No cases matched, this shouldn't happen!\n");
			assert(0);
			return;
		}

		// This bit indicates whether to lock or revert to an unsynchronized state when synchronization is lost
		// (See Section 11 on Universe Synchronization and 11.1 for discussion on synchronization states).
		// When set to 0, components that had been operating in a synchronized state shall not update with any
		// new packets until synchronization resumes. When set to 1, once synchronization has been lost,
		// components that had been operating in a synchronized state need not wait for a new
		// E1.31 Synchronization Packet in order to update to the next E1.31 Data Packet.
		if ((m_E131.E131Packet.Data.FrameLayer.Options & OptionsMask::FORCE_SYNCHRONIZATION) == 0) {
			// 6.3.3.1 Synchronization Address Usage in an E1.31 Synchronization Packet
			// An E1.31 Synchronization Packet is sent to synchronize the E1.31 data on a specific universe number.
			// A Synchronization Address of 0 is thus meaningless, and shall not be transmitted.
			// Receivers shall ignore E1.31 Synchronization Packets containing a Synchronization Address of 0.
			if (m_E131.E131Packet.Data.FrameLayer.SynchronizationAddress != 0) {
				if (!m_State.IsForcedSynchronized) {
					if (!(isSourceA || isSourceB)) {
						SetSynchronizationAddress((pSourceA->nIp != 0), (pSourceB->nIp != 0), __builtin_bswap16(m_E131.E131Packet.Data.FrameLayer.SynchronizationAddress));
					} else {
						SetSynchronizationAddress(isSourceA, isSourceB, __builtin_bswap16(m_E131.E131Packet.Data.FrameLayer.SynchronizationAddress));
					}
					m_State.IsForcedSynchronized = true;
					m_State.IsSynchronized = true;
				}
			}
		} else {
			m_State.IsForcedSynchronized = false;
		}

		if ((!m_State.IsSynchronized) || (m_State.bDisableSynchronize)) {
			lightset::Data::Output(m_pLightSet, nPortIndex);

			if (!m_OutputPort[nPortIndex].IsTransmitting) {
				m_pLightSet->Start(nPortIndex);
				m_State.IsChanged = true;
				m_OutputPort[nPortIndex].IsTransmitting = true;
			}
		}

		m_State.nReceivingDmx |= (1U << static_cast<uint8_t>(lightset::PortDir::OUTPUT));
	}
}

void E131Bridge::SetNetworkDataLossCondition(bool bSourceA, bool bSourceB) {
	DEBUG_ENTRY
	DEBUG_PRINTF("%d %d", bSourceA, bSourceB);

	m_State.IsChanged = true;
	auto doFailsafe = false;

	if (bSourceA && bSourceB) {
		m_State.IsNetworkDataLoss = true;
		m_State.IsMergeMode = false;
		m_State.IsSynchronized = false;
		m_State.IsForcedSynchronized = false;
		m_State.nPriority = priority::LOWEST;

		for (uint32_t i = 0; i < e131bridge::MAX_PORTS; i++) {
			if (m_OutputPort[i].IsTransmitting) {
				doFailsafe = true;
				m_OutputPort[i].sourceA.nIp = 0;
				memset(m_OutputPort[i].sourceA.cid, 0, e131::CID_LENGTH);
				m_OutputPort[i].sourceB.nIp = 0;
				memset(m_OutputPort[i].sourceB.cid, 0, e131::CID_LENGTH);
				lightset::Data::ClearLength(i);
				m_OutputPort[i].IsDataPending = false;
				m_OutputPort[i].IsTransmitting = false;
				m_OutputPort[i].IsMerging = false;
			}
		}
	} else {
		for (uint32_t i = 0; i < e131bridge::MAX_PORTS; i++) {
			if (m_OutputPort[i].IsTransmitting) {
				if ((bSourceA) && (m_OutputPort[i].sourceA.nIp != 0)) {
					m_OutputPort[i].sourceA.nIp = 0;
					memset(m_OutputPort[i].sourceA.cid, 0, e131::CID_LENGTH);
					m_OutputPort[i].IsMerging = false;
				}
				if ((bSourceB) && (m_OutputPort[i].sourceB.nIp != 0)) {
					m_OutputPort[i].sourceB.nIp = 0;
					memset(m_OutputPort[i].sourceB.cid, 0, e131::CID_LENGTH);
					m_OutputPort[i].IsMerging = false;
				}
				if (!m_State.IsMergeMode) {
					doFailsafe = true;
					lightset::Data::ClearLength(i);
					m_OutputPort[i].IsDataPending = false;
					m_OutputPort[i].IsTransmitting = false;
				}
			}
		}
	}

	if (doFailsafe) {
		switch (m_State.failsafe) {
		case lightset::FailSafe::HOLD:
			break;
		case lightset::FailSafe::OFF:
			m_pLightSet->Blackout(true);
			break;
		case lightset::FailSafe::ON:
			m_pLightSet->FullOn();
			break;
		default:
			assert(0);
			__builtin_unreachable();
			break;
		}
	}

	LedBlink::Get()->SetMode(ledblink::Mode::NORMAL);

	m_State.nReceivingDmx &= static_cast<uint8_t>(~(1U << static_cast<uint8_t>(lightset::PortDir::OUTPUT)));

	DEBUG_EXIT
}

bool E131Bridge::IsValidRoot() {
	// 5 E1.31 use of the ACN Root Layer Protocol
	// Receivers shall discard the packet if the ACN Packet Identifier is not valid.
	if (memcmp(m_E131.E131Packet.Raw.RootLayer.ACNPacketIdentifier, E117Const::ACN_PACKET_IDENTIFIER, e117::PACKET_IDENTIFIER_LENGTH) != 0) {
		return false;
	}
	
	if (m_E131.E131Packet.Raw.RootLayer.Vector != __builtin_bswap32(vector::root::DATA)
			 && (m_E131.E131Packet.Raw.RootLayer.Vector != __builtin_bswap32(vector::root::EXTENDED)) ) {
		return false;
	}

	return true;
}

bool E131Bridge::IsValidDataPacket() {
	// DMP layer

	// The DMP Layer's Vector shall be set to 0x02, which indicates a DMP Set Property message by
	// transmitters. Receivers shall discard the packet if the received value is not 0x02.
	if (m_E131.E131Packet.Data.DMPLayer.Vector != e131::vector::dmp::SET_PROPERTY) {
		return false;
	}

	// Transmitters shall set the DMP Layer's Address Type and Data Type to 0xa1. Receivers shall discard the
	// packet if the received value is not 0xa1.
	if (m_E131.E131Packet.Data.DMPLayer.Type != 0xa1) {
		return false;
	}

	// Transmitters shall set the DMP Layer's First Property Address to 0x0000. Receivers shall discard the
	// packet if the received value is not 0x0000.
	if (m_E131.E131Packet.Data.DMPLayer.FirstAddressProperty != __builtin_bswap16(0x0000)) {
		return false;
	}

	// Transmitters shall set the DMP Layer's Address Increment to 0x0001. Receivers shall discard the packet if
	// the received value is not 0x0001.
	if (m_E131.E131Packet.Data.DMPLayer.AddressIncrement != __builtin_bswap16(0x0001)) {
		return false;
	}

	return true;
}

void E131Bridge::Run() {
	uint16_t nForeignPort;

	const auto nBytesReceived = Network::Get()->RecvFrom(m_nHandle, &m_E131.E131Packet, sizeof(m_E131.E131Packet), &m_E131.IPAddressFrom, &nForeignPort) ;

	m_nCurrentPacketMillis = Hardware::Get()->Millis();

	if (__builtin_expect((nBytesReceived == 0), 1)) {
		if (m_State.nActiveOutputPorts != 0) {
			if ((m_nCurrentPacketMillis - m_nPreviousPacketMillis) >= static_cast<uint32_t>(NETWORK_DATA_LOSS_TIMEOUT_SECONDS * 1000)) {
				if ((m_pLightSet != nullptr) && (!m_State.IsNetworkDataLoss)) {
					SetNetworkDataLossCondition();
					DEBUG_PUTS("");
				}
			}

			if ((m_nCurrentPacketMillis - m_nPreviousPacketMillis) >= 1000) {
				m_State.nReceivingDmx &= static_cast<uint8_t>(~(1U << static_cast<uint8_t>(lightset::PortDir::OUTPUT)));
			}
		}

		if (m_pE131DmxIn != nullptr) {
			HandleDmxIn();
			SendDiscoveryPacket();
		}

		// The ledblink::Mode::FAST is for RDM Identify (Art-Net 4)
		if (m_bEnableDataIndicator && (LedBlink::Get()->GetMode() != ledblink::Mode::FAST)) {
			if (m_State.nReceivingDmx != 0) {
				LedBlink::Get()->SetMode(ledblink::Mode::DATA);
			} else {
				LedBlink::Get()->SetMode(ledblink::Mode::NORMAL);
			}
		}

		return;
	}

	if (__builtin_expect((!IsValidRoot()), 0)) {
		return;
	}

	m_State.IsNetworkDataLoss = false;
	m_nPreviousPacketMillis = m_nCurrentPacketMillis;

	if (m_State.IsSynchronized && !m_State.IsForcedSynchronized) {
		if ((m_nCurrentPacketMillis - m_State.SynchronizationTime) >= static_cast<uint32_t>(NETWORK_DATA_LOSS_TIMEOUT_SECONDS * 1000)) {
			m_State.IsSynchronized = false;
		}
	}

	if (m_pLightSet != nullptr) {
		const auto nRootVector = __builtin_bswap32(m_E131.E131Packet.Raw.RootLayer.Vector);

		if (nRootVector == vector::root::DATA) {
			if (IsValidDataPacket()) {
				HandleDmx();
			}
		} else if (nRootVector == vector::root::EXTENDED) {
			const auto nFramingVector = __builtin_bswap32(m_E131.E131Packet.Raw.FrameLayer.Vector);
				if (nFramingVector == vector::extended::SYNCHRONIZATION) {
				HandleSynchronization();
			}
		} else {
			DEBUG_PRINTF("Not supported Root Vector : 0x%x", nRootVector);
		}
	}

	if (m_pE131DmxIn != nullptr) {
		HandleDmxIn();
		SendDiscoveryPacket();
	}

	// The ledblink::Mode::FAST is for RDM Identify (Art-Net 4)
	if (m_bEnableDataIndicator && (LedBlink::Get()->GetMode() != ledblink::Mode::FAST)) {
		if (m_State.nReceivingDmx != 0) {
			LedBlink::Get()->SetMode(ledblink::Mode::DATA);
		} else {
			LedBlink::Get()->SetMode(ledblink::Mode::NORMAL);
		}
	}
}
