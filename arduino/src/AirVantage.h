/*
  Copyright (c) 2013 Arduino LLC. All right reserved.

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

#ifndef _AIR_VANTAGE_CLASS_H_INCLUDED_
#define _AIR_VANTAGE_CLASS_H_INCLUDED_

#include <Bridge.h>

typedef enum _storage_e {
  CACHE = 0,
  PERSIST,
  PERSIST_ENCRYPT,
} storage_e;

class AirVantageClass {
  public:
    AirVantageClass(BridgeClass &b = Bridge) : bridge(b) { }

    void begin() { }
    void end() { }

    // Start Session
    void startSession(const String&, const String&, uint8_t, storage_e);

    // EndSession
    void endSession(void);

    // Susbscribe
    void subscribe(const String&);

    // Push Boolean
    void pushBoolean(const String&, uint8_t);

    // Push Integer
    void pushInteger(const String&, int);

    // Push Float
    void pushFloat(const String&, uint8_t, float);

    // Push String
    void pushString(const String&, const String&);

    // Return the size of data available 
    unsigned int dataAvailable(void);

    // Receive a message and store it inside a buffer
    unsigned int readMessage(uint8_t*, unsigned int size);
    // Receive a message and store it inside a String
    void readMessage(String&, unsigned int maxLength = 128);

    String parseTimestamp(const String&, const String&);
    bool parseBoolean(const String&, const String&);
    long parseInteger(const String&, const String&);
    float parseFloat(const String&, const String&);
    String parseString(const String&, const String&);
    
  private:
    BridgeClass &bridge;
};

extern AirVantageClass AirVantage;

#endif // _AIR_VANTAGE_CLASS_H_INCLUDED_
