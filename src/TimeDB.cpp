#include "TimeDB.h"

TimeDB::TimeDB(String apiKey)
{
}

time_t TimeDB::getTime()
{
  WiFiClient client;
  String apiGetData = "GET /v2.1/get-time-zone?key=V4AL3Z4VF8D3&format=json&by=zone&zone=Europe/Oslo HTTP/1.1";
  String result = "";
  if (client.connect(servername, 80))
  { // starts client connection, checks for connection
    client.println(apiGetData);
    client.println("Host: " + String(servername));
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
  }
  else
  {
    Serial.println("connection for time data failed"); // error message if no client connect
    Serial.println();
    return 20;
  }

  while (client.connected() && !client.available())
    delay(1); // waits for data

  Serial.println("Waiting for data");

  boolean record = false;
  while (client.connected() || client.available())
  {                         // connected or data available
    char c = client.read(); // gets byte from ethernet buffer
    if (String(c) == "{")
    {
      record = true;
    }
    if (record)
    {
      result = result + c;
    }
    if (String(c) == "}")
    {
      record = false;
    }
  }
  client.stop(); // stop client
  int TStart = result.lastIndexOf('{');
  result = result.substring(TStart);

  char jsonArray[result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  DynamicJsonBuffer json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  Serial.println();
  Serial.print("Timestamp of time fetch: ");
  Serial.println(root["formatted"].as<const char *>());
  if (root["timestamp"] == 0)
  {
    return 20;
  }
  else
  {
    return (unsigned long)root["timestamp"];
  }
}

String TimeDB::zeroPad(int number)
{
  if (number < 10)
  {
    return "0" + String(number);
  }
  else
  {
    return String(number);
  }
}
