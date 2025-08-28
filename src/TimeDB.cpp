#include "TimeDB.h"

TimeDB::TimeDB(String apiKey) : apiKey(apiKey)
{
}

time_t TimeDB::getTime()
{
  WiFiClient client;

  // Use the stored API key and timezone from Settings.h
  String apiGetData = "GET /v2.1/get-time-zone?key=" + apiKey + "&format=json&by=zone&zone=Europe/Oslo HTTP/1.1";
  String result = "";

  Serial.println("Connecting to time server...");

  if (!client.connect(servername, 80))
  {
    Serial.println("Connection to time server failed");
    return INVALID_TIME;
  }

  // Send HTTP request
  client.println(apiGetData);
  client.println("Host: " + String(servername));
  client.println("User-Agent: ESP8266-Clock/1.0");
  client.println("Connection: close");
  client.println();

  // Wait for response with timeout
  unsigned long timeout = millis();
  while (client.connected() && !client.available())
  {
    if (millis() - timeout > CONNECTION_TIMEOUT)
    {
      Serial.println("Timeout waiting for server response");
      client.stop();
      return INVALID_TIME;
    }
    delay(1);
  }

  Serial.println("Reading response from server...");

  // Read response
  bool jsonStarted = false;
  while (client.connected() || client.available())
  {
    char c = client.read();
    if (c == '{')
    {
      jsonStarted = true;
    }
    if (jsonStarted)
    {
      result += c;
    }
    if (c == '}')
    {
      break; // Complete JSON received
    }
  }
  client.stop();

  if (result.length() == 0)
  {
    Serial.println("No valid JSON response received");
    return INVALID_TIME;
  }

  // Parse JSON using newer ArduinoJson library
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, result);

  if (error)
  {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return INVALID_TIME;
  }

  if (!doc.containsKey("timestamp"))
  {
    Serial.println("Timestamp not found in response");
    return INVALID_TIME;
  }

  unsigned long timestamp = doc["timestamp"];
  if (timestamp == 0)
  {
    Serial.println("Invalid timestamp received");
    return INVALID_TIME;
  }

  Serial.print("Time fetched successfully: ");
  Serial.println(doc["formatted"].as<String>());

  return timestamp;
}

String TimeDB::zeroPad(int number)
{
  return (number < 10) ? "0" + String(number) : String(number);
}
