/*
  Copyright (c) 2016 Sierra Wireless. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _SWI_BRIDGE_CLASS_H_INCLUDED_
#define _SWI_BRIDGE_CLASS_H_INCLUDED_

#include <Bridge.h>

typedef enum _storage_e {
  CACHE = 0,
  PERSIST,
  PERSIST_ENCRYPT,
} storage_e;

class SwiBridgeClass {
  public:
    SwiBridgeClass(BridgeClass &b = Bridge) : bridge(b) { }

    void begin() { }
    void end() { }

    // Start Session with mangOH
    void startSession(const String&, const String&, uint8_t, storage_e);

    // EndSession with mangOH
    void endSession(void);

    // Susbscribe to receive data updates
    void subscribe(const String&);

    // Push Boolean value
    void pushBoolean(const String&, uint8_t);

    // Push Integer value
    void pushInteger(const String&, int);

    // Push Float value
    void pushFloat(const String&, uint8_t, float);

    // Push String value
    void pushString(const String&, const String&);

    // Return the size of data available
    unsigned int dataAvailable(void);

    // Receive a message and store it inside a buffer
    unsigned int readMessage(uint8_t*, unsigned int size);

    // Receive a message and store it inside a String
    void readMessage(String&, unsigned int maxLength = 128);

    // Parse timestamp from received message
    String parseTimestamp(const String&, const String&);

    // Parse Boolean value from received message
    bool parseBoolean(const String&, const String&);

    // Parse Integer value from received message
    long parseInteger(const String&, const String&);

    // Parse Float value from received message
    float parseFloat(const String&, const String&);

    // Parse String value from received message
    String parseString(const String&, const String&);

  private:
    BridgeClass &bridge;
};

extern SwiBridgeClass SwiBridge;

#endif // _SWI_BRIDGE_CLASS_H_INCLUDED_
