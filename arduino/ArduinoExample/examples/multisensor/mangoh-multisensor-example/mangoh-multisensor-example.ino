<?xml version="1.0" encoding="ISO-8859-1"?>

<app:application
 xmlns:app="http://www.sierrawireless.com/airvantage/application/1.0"
        type="mangoh testing"
        name="mangOH testing"
        revision="0.0.2">
 <capabilities>

  <communication>
   <protocol comm-id="IMEI" type="MQTT" />
  </communication>

  <data>
   <encoding type="MQTT">
    <asset default-label="Greenhouse" id="greenhouse">
      <variable default-label="Temperature" path="temperature"
                                      type="double"/>
      <variable default-label="Humidity" path="humidity"
                                      type="double"/>
      <variable default-label="Luminosity" path="luminosity"
                                      type="int"/>
      <variable default-label="Noise" path="noise"
                                      type="int"/>
      <variable default-label="Water" path="water"
                                      type="boolean"/>
      <variable default-label="Dust" path="dust"
                                      type="double"/>
      <variable default-label="Oxygen" path="oxygen"
                                      type="double"/>

     </asset>
   </encoding>
  </data>
  </capabilities>
</app:application>
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

#include <SwiBridge.h>

void SwiBridgeClass::startSession(const String& url, const String& pw, uint8_t avPush, storage_e storage)
{
  uint8_t urlLen = url.length();
  uint8_t cmd[] = {'0', avPush, storage, urlLen };
  bridge.transfer(cmd, 4, (const uint8_t*)url.c_str(), url.length(), (const uint8_t*)pw.c_str(), pw.length(), NULL, 0);
}

void SwiBridgeClass::endSession(void) {
  uint8_t cmd[] = {'1'};
  bridge.transfer(cmd, 1, NULL, 0);
}

void SwiBridgeClass::subscribe(const String& name) {
  uint8_t nameLen = name.length();
  uint8_t cmd[] = {'2', nameLen };
  bridge.transfer(cmd, 2, (const uint8_t*)name.c_str(), name.length(), NULL, 0);
}

void SwiBridgeClass::pushBoolean(const String& name, uint8_t value) {
  uint8_t nameLen = name.length();
  uint8_t cmd[] = {'3', nameLen };
  bridge.transfer(cmd, 2, (const uint8_t*)name.c_str(), name.length(), (uint8_t*)&value, sizeof(value), NULL, 0);
}

void SwiBridgeClass::pushInteger(const String& name, int value) {
  uint8_t nameLen = name.length();
  uint8_t cmd[] = {'4', nameLen };
  bridge.transfer(cmd, 2, (const uint8_t*)name.c_str(), name.length(), (uint8_t*)&value, sizeof(value), NULL, 0);
}

void SwiBridgeClass::pushFloat(const String& name, uint8_t precision, float value) {
  struct floatVal {
    uint8_t precision;
    int32_t val;
  };
  struct floatVal fVal = {0};
  uint8_t i = 0;
  uint8_t nameLen = name.length();
  uint8_t cmd[] = {'5', nameLen };

  fVal.precision = precision;
  for (i = 0; i < precision; i++) value *= 10;
  fVal.val = value;
  bridge.transfer(cmd, 2, (const uint8_t*)name.c_str(), name.length(), (uint8_t*)&fVal, sizeof(fVal), NULL, 0);
}

void SwiBridgeClass::pushString(const String& name, const String& value) {
  uint8_t nameLen = name.length();
  uint8_t cmd[] = {'6', nameLen };
  bridge.transfer(cmd, 2, (const uint8_t*)name.c_str(), name.length(), (const uint8_t*)value.c_str(), value.length(), NULL, 0);
}

unsigned int SwiBridgeClass::dataAvailable() {
  uint8_t tmp[] = {'7'};
  uint8_t res[2] = {0};
  bridge.transfer(tmp, 1, res, 2);
  return (res[0] << 8) + res[1];
}

unsigned int SwiBridgeClass::readMessage(uint8_t *buff, unsigned int size) {
  uint8_t tmp[] = {'8'};
  return bridge.transfer(tmp, 1, buff, size);
}

void SwiBridgeClass::readMessage(String &str, unsigned int maxLength) {
  uint8_t tmp[] = {'8'};
  // XXX: Is there a better way to create the string?
  uint8_t buff[maxLength + 1];
  int l = bridge.transfer(tmp, 1, buff, maxLength);
  buff[l] = 0;
  str = (const char *)buff;
}

String SwiBridgeClass::parseTimestamp(const String& name, const String& msg) {
  String ret;

  int32_t nameIdx = msg.indexOf(name);
  if (msg.startsWith("|") && (nameIdx != -1)) {
    String valueStr = msg.substring(nameIdx + name.length() + 1);
    ret = valueStr.substring(valueStr.indexOf('|') + 1);
    if (ret.indexOf('|') != -1) {
      ret = ret.substring(0, ret.indexOf('|'));
    }
  }

  return ret;
}

bool SwiBridgeClass::parseBoolean(const String& name, const String& msg) {
  bool ret = false;

  int32_t nameIdx = msg.indexOf(name);
  if (msg.startsWith("|") && (nameIdx != -1)) {
    String valueStr = msg.substring(nameIdx + name.length() + 1);
    if (valueStr.indexOf('|') != -1) {
      valueStr = valueStr.substring(0, valueStr.indexOf('|'));
    }

    ret = valueStr.toInt() ? true:false;
  }

  return ret;
}

long SwiBridgeClass::parseInteger(const String& name, const String& msg) {
  long ret = 0;

  int32_t nameIdx = msg.indexOf(name);
  if (msg.startsWith("|") && (nameIdx != -1)) {
    String valueStr = msg.substring(nameIdx + name.length() + 1);
    if (valueStr.indexOf('|') != -1) {
      valueStr = valueStr.substring(0, valueStr.indexOf('|'));
    }

    ret = valueStr.toInt();
  }

  return ret;
}

float SwiBridgeClass::parseFloat(const String& name, const String& msg) {
  float ret = 0.0;

  int32_t nameIdx = msg.indexOf(name);
  if (msg.startsWith("|") && (nameIdx != -1)) {
    String valueStr = msg.substring(nameIdx + name.length() + 1);
    if (valueStr.indexOf('|') != -1) {
      valueStr = valueStr.substring(0, valueStr.indexOf('|'));
    }
    ret = valueStr.toFloat();
  }

  return ret;
}

String SwiBridgeClass::parseString(const String& name, const String& msg) {
  String ret;

  int32_t nameIdx = msg.indexOf(name);
  if (msg.startsWith("|") && (nameIdx != -1)) {
    String valueStr = msg.substring(nameIdx + name.length() + 1);
    if (valueStr.indexOf('|') != -1) {
      ret = valueStr.substring(0, valueStr.indexOf('|'));
    }
  }

  return ret;
}

SwiBridgeClass SwiBridge(Bridge);
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

    // Start Session with Mangoh
    void startSession(const String&, const String&, uint8_t, storage_e);

    // EndSession with Mangoh
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
