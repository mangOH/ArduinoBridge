<?xml version="1.0" encoding="ISO-8859-1"?>
<app:application
    xmlns:app="http://www.sierrawireless.com/airvantage/application/1.0"
    type="mangoh testing"
    name="mangOH testing"
    revision="0.0.2">
  <capabilities>
    <communication><protocol comm-id="IMEI" type="MQTT" /></communication>
    <data>
      <encoding type="MQTT">
        <asset default-label="Greenhouse" id="greenhouse">
          <variable default-label="Temperature" path="temperature" type="double"/>
          <variable default-label="Humidity" path="humidity" type="double"/>
          <variable default-label="Luminosity" path="luminosity" type="int"/>
          <variable default-label="Noise" path="noise" type="int"/>
          <variable default-label="Water" path="water" type="boolean"/>
          <variable default-label="Dust" path="dust" type="double"/>
          <variable default-label="Oxygen" path="oxygen" type="double"/>
        </asset>
      </encoding>
    </data>
  </capabilities>
</app:application>
