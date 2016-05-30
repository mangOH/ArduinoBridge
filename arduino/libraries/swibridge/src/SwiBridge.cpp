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
