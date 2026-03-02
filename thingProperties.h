#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include "arduino_secrets.h"

// Callbacks (deben existir en el .ino)
void onChatChange();
void onNotificationChange();
void onLed2Change();

// Cloud properties
String chat;
String notification;
bool led2;

WiFiConnectionHandler ArduinoIoTPreferredConnection(SECRET_SSID, SECRET_OPTIONAL_PASS);

void initProperties() {
  ArduinoCloud.addProperty(chat, READWRITE, ON_CHANGE, onChatChange);
  ArduinoCloud.addProperty(notification, READWRITE, ON_CHANGE, onNotificationChange);
  ArduinoCloud.addProperty(led2, READWRITE, ON_CHANGE, onLed2Change);
}
