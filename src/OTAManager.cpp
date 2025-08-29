#include "OTAManager.h"
#include "Settings.h"

// Static member initialization
OTAManager *OTAManager::instance = nullptr;

OTAManager::OTAManager(DisplayManager &displayRef) : display(displayRef)
{
  instance = this;
}

void OTAManager::initialize()
{
  // Setup mDNS
  if (MDNS.begin(DEVICE_HOSTNAME.c_str()))
  {
    Serial.println("mDNS responder started");
  }

  // Setup OTA
  ArduinoOTA.setHostname(DEVICE_HOSTNAME.c_str());
  ArduinoOTA.setPassword("mqtt-clock-ota"); // Change this to your preferred password

  ArduinoOTA.onStart(onStart);
  ArduinoOTA.onEnd(onEnd);
  ArduinoOTA.onProgress(onProgress);
  ArduinoOTA.onError(onError);

  ArduinoOTA.begin();

  Serial.println("OTA update ready");
  Serial.println("Hostname: " + DEVICE_HOSTNAME);
  Serial.println("OTA Password: mqtt-clock-ota");
}

void OTAManager::loop()
{
  ArduinoOTA.handle();
  MDNS.update();
}

void OTAManager::onStart()
{
  String type;
  if (ArduinoOTA.getCommand() == U_FLASH)
  {
    type = "sketch";
  }
  else // U_SPIFFS
  {
    type = "filesystem";
  }

  Serial.println("Start updating " + type);

  if (instance)
  {
    instance->display.fillScreen(false);
    instance->display.centerPrint("OTA");
    instance->display.write();
  }
}

void OTAManager::onEnd()
{
  Serial.println("\nEnd");

  if (instance)
  {
    instance->display.fillScreen(false);
    instance->display.centerPrint("Done");
    instance->display.write();
    delay(1000);
  }
}

void OTAManager::onProgress(unsigned int progress, unsigned int total)
{
  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));

  if (instance)
  {
    int percent = progress / (total / 100);
    String progressText = String(percent) + "%";
    instance->display.fillScreen(false);
    instance->display.centerPrint(progressText);
    instance->display.write();
  }
}

void OTAManager::onError(ota_error_t error)
{
  Serial.printf("Error[%u]: ", error);
  String errorMsg = "Err";

  if (error == OTA_AUTH_ERROR)
  {
    Serial.println("Auth Failed");
    errorMsg = "Auth";
  }
  else if (error == OTA_BEGIN_ERROR)
  {
    Serial.println("Begin Failed");
    errorMsg = "Begin";
  }
  else if (error == OTA_CONNECT_ERROR)
  {
    Serial.println("Connect Failed");
    errorMsg = "Conn";
  }
  else if (error == OTA_RECEIVE_ERROR)
  {
    Serial.println("Receive Failed");
    errorMsg = "Recv";
  }
  else if (error == OTA_END_ERROR)
  {
    Serial.println("End Failed");
    errorMsg = "End";
  }

  if (instance)
  {
    instance->display.fillScreen(false);
    instance->display.centerPrint(errorMsg);
    instance->display.write();
  }
}
