/**
 * @file midi_description.h
 */
/* Copyright (C) 2016 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef MIDI_DESCRIPTION_H_
#define MIDI_DESCRIPTION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*@observer@*/extern const char *midi_description_get_type(uint8_t);
/*@observer@*/extern const char *midi_description_get_control_change(uint8_t);
/*@observer@*/extern const char *midi_description_get_control_function(uint8_t);
/*@observer@*/extern const char *midi_description_get_key_name(uint8_t);
/*@observer@*/extern const char *midi_description_get_drum_kit_name(uint8_t);
/*@observer@*/extern const char *midi_description_get_instrument_name(uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_DESCRIPTION_H_ */
