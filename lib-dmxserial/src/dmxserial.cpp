/**
 * @file dmxserial.cpp
 *
 */
/* Copyright (C) 2020 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>

#include "dmxserial.h"
#include "dmxserial_internal.h"
#include "dmxserialtftp.h"

#include "lightset.h"

#include "network.h"

#include "debug.h"

DmxSerial *DmxSerial::s_pThis = 0;

DmxSerial::DmxSerial(void): m_nFilesCount(0), m_nDmxLastSlot(DMX_UNIVERSE_SIZE), m_bEnableTFTP(false), m_pDmxSerialTFTP(0), m_nHandle(-1) {
	assert(s_pThis == 0);
	s_pThis = this;

	for (uint32_t i = 0; i < DMXSERIAL_FILE_MAX_NUMBER; i++) {
		m_aFileIndex[i] = -1;
		m_pDmxSerialChannelData[i] = 0;
	}

	memset(m_DmxData, 0, sizeof(m_DmxData));
}

DmxSerial::~DmxSerial(void) {
	for (uint32_t i = 0; i < m_nFilesCount; i++) {
		if (m_pDmxSerialChannelData[i] != 0) {
			delete m_pDmxSerialChannelData[i];
		}
	}

	Network::Get()->End(UDP_PORT);

	s_pThis = 0;
}

void DmxSerial::Init(void) {
	// UDP Request
	m_nHandle = Network::Get()->Begin(UDP_PORT);
	assert(m_nHandle != -1);

	ScanDirectory();
	m_Serial.Init();
}

void DmxSerial::Start(uint8_t nPort) {
	// No actions here
}

void DmxSerial::Stop(uint8_t nPort) {
	// No actions here
}

void DmxSerial::SetData(uint8_t nPort, const uint8_t *pData, uint16_t nLength) {

	for (uint32_t nIndex = 0; nIndex < m_nFilesCount; nIndex++) {
		const uint32_t nOffset = m_aFileIndex[nIndex] - 1;

		if (m_DmxData[nOffset] != pData[nOffset]) {
			m_DmxData[nOffset] = pData[nOffset];

//			DEBUG_PRINTF("nPort=%d, nIndex=%d, m_aFileIndex[nIndex]=%d, nOffset=%d, m_DmxData[nOffset]=%d", nPort, nIndex, m_aFileIndex[nIndex], nOffset, m_DmxData[nOffset]);

			if (m_pDmxSerialChannelData[nIndex] != 0) {
				uint32_t nLength;
				const uint8_t *pSerialData = m_pDmxSerialChannelData[nIndex]->GetData(m_DmxData[nOffset], nLength);

				if (nLength == 0) {
					continue;
				}

				m_Serial.Send(pSerialData, nLength);
			}
		}
	}
}

void DmxSerial::Print(void) {
	m_Serial.Print();

	printf("Files : %d\n", m_nFilesCount);
	printf("DMX\n");
	printf(" First channel : %d\n", m_aFileIndex[0]);
	printf(" Last channel  : %d\n", m_nDmxLastSlot);
}

void DmxSerial::ScanDirectory(void) {
	// We can only run this once, for now
	assert(m_pDmxSerialChannelData[0] == 0);

    DIR *dirp;
    struct dirent *dp;
    m_nFilesCount = 0;

    if ((dirp = opendir(".")) == NULL) {
		perror("couldn't open '.'");

		for (uint32_t i = 0; i < DMXSERIAL_FILE_MAX_NUMBER; i++) {
			m_aFileIndex[i] = -1;
		}

		return;
	}

    do {
        if ((dp = readdir(dirp)) != NULL) {
        	if (dp->d_type == DT_DIR) {
        		continue;
        	}

          	uint16_t nFileNumber;
        	if (!CheckFileName(dp->d_name, nFileNumber)) {
                continue;
            }

        	m_aFileIndex[m_nFilesCount] = nFileNumber;

            DEBUG_PRINTF("[%d] found %s", m_nFilesCount, dp->d_name);

            m_nFilesCount++;

            if (m_nFilesCount == DMXSERIAL_FILE_MAX_NUMBER) {
            	break;
            }
        }
    } while (dp != NULL);

    // Sort
	for (uint32_t i = 0; i < m_nFilesCount; i++) {
		for (uint32_t j = 0; j < m_nFilesCount; j++) {
			if (m_aFileIndex[j] > m_aFileIndex[i]) {
				int16_t tmp = m_aFileIndex[i];
				m_aFileIndex[i] = m_aFileIndex[j];
				m_aFileIndex[j] = tmp;
			}
		}
	}

	m_nDmxLastSlot = m_aFileIndex[m_nFilesCount - 1];

	for (uint32_t i = m_nFilesCount; i < DMXSERIAL_FILE_MAX_NUMBER; i++) {
		m_aFileIndex[i] = -1;
	}

	static_cast<void>(closedir(dirp));

#ifndef NDEBUG
	printf("%d\n", m_nFilesCount);
#endif

	for (uint32_t nIndex = 0; nIndex < m_nFilesCount; nIndex++) {
#ifndef NDEBUG
		printf("\tnIndex=%d -> %d\n", nIndex, m_aFileIndex[nIndex]);
#endif

//		if (m_pDmxSerialChannelData[nIndex] != 0) {
//			delete m_pDmxSerialChannelData[nIndex];
//			m_pDmxSerialChannelData[nIndex] = 0;
//		}

		m_pDmxSerialChannelData[nIndex] = new DmxSerialChannelData;
		assert(m_pDmxSerialChannelData[nIndex] != 0);

		char pBuffer[16];
		snprintf(pBuffer, sizeof(pBuffer) - 1, DMXSERIAL_FILE_PREFIX "%.3d" DMXSERIAL_FILE_SUFFIX, m_aFileIndex[nIndex]);
		DEBUG_PUTS(pBuffer);
		m_pDmxSerialChannelData[nIndex]->Parse(pBuffer);

//		if (!m_pDmxSerialChannelData[nIndex]->Parse(pBuffer)) {
//			DEBUG_PUTS("Parse error");
//			delete m_pDmxSerialChannelData[nIndex];
//			m_pDmxSerialChannelData[nIndex] = 0;
//		}
	}

#ifndef NDEBUG
	for (uint32_t nIndex = 0; nIndex < m_nFilesCount; nIndex++) {
		printf("\tnIndex=%d -> %d\n", nIndex, m_aFileIndex[nIndex]);
		m_pDmxSerialChannelData[nIndex]->Dump();
	}
#endif
}

void DmxSerial::EnableTFTP(bool bEnableTFTP) {
	DEBUG_ENTRY

	if (bEnableTFTP == m_bEnableTFTP) {
		DEBUG_EXIT
		return;
	}

	DEBUG_PRINTF("bEnableTFTP=%d", bEnableTFTP);

	m_bEnableTFTP = bEnableTFTP;

	if (m_bEnableTFTP) {
		assert(m_pDmxSerialTFTP == 0);
		m_pDmxSerialTFTP = new DmxSerialTFTP;
		assert(m_pDmxSerialTFTP != 0);
	} else {
		assert(m_pDmxSerialTFTP != 0);
		delete m_pDmxSerialTFTP;
		m_pDmxSerialTFTP = 0;
	}

	DEBUG_EXIT
}

void DmxSerial::Run(void) {
	HandleUdp();

	if (m_pDmxSerialTFTP == 0) {
		return;
	}

	m_pDmxSerialTFTP->Run();
}

bool DmxSerial::DeleteFile(uint16_t nFileNumber) {
	DEBUG_PRINTF("nFileNumber=%u", nFileNumber);

	char aFileName[DMXSERIAL_FILE_NAME_LENGTH + 1];

	if (FileNameCopyTo(aFileName, sizeof(aFileName), nFileNumber)) {
		const int nResult = unlink(aFileName);
		DEBUG_PRINTF("nResult=%d", nResult);
		DEBUG_EXIT
		return (nResult == 0);
	}

	DEBUG_EXIT
	return false;
}

bool DmxSerial::DeleteFile(const char *pFileNumber) {
	DEBUG_PUTS(pFileNumber);

	if (strlen(pFileNumber) != 3) {
		return false;
	}

	uint16_t nFileNumber = (pFileNumber[0] - '0') * 100;
	nFileNumber += (pFileNumber[1] - '0') * 10;
	nFileNumber += (pFileNumber[2] - '0');

	if (nFileNumber > DMXSERIAL_FILE_MAX_NUMBER) {
		return false;
	}

	return DeleteFile(nFileNumber);
}
