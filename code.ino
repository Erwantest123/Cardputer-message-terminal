#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "M5Cardputer.h"
#include "M5GFX.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
SemaphoreHandle_t displayMutex;

// Objects and settings
M5Canvas canvas(&M5Cardputer.Display);
String inputData = "> ";
const char* serverUrl = "http://176.174.44.98/"; // Server address
WiFiClient client;

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 3600; // UTC+1 for France
WiFiUDP udp;
NTPClient timeClient(udp, ntpServer, utcOffsetInSeconds);

// Auto-scroll control
int messageStartIndex = 7; // Index of the first visible message
bool autoScrollEnabled = true; // Enable auto-scroll by default
String lastFileContent = ""; // Dernière version connue du fichier

// Déclaration de la tâche FreeRTOS
void checkFileTask(void* parameter);

uint16_t colorMap[] = {WHITE, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};
int color = 0; // Default color is white
int horizontalScrollOffset = 0; // Offset pour défilement horizontal
String userName = "";  // Variable pour stocker le nom de l'utilisateur
int username;
int newmessageled = 1;
int buttonstatus = 0;
int lastButtonStatus = 0;  // Stocke la dernière valeur

Adafruit_NeoPixel pixels(2, 21, NEO_GRB + NEO_KHZ800);


void luxTask(void *pvParameters) {
  int timer = 30000;
  int lux = 255;
 while (true) {
  timer--;
  if(timer == 15000) {
    Serial.print("lightdown 15sec");
    for (int i = 0; i < 240; i++) {
    lux--;
    M5.Lcd.setBrightness(lux);  // Appliquer la luminosité maximale
vTaskDelay(6 / portTICK_PERIOD_MS);
}
  timer = 13000;
  }
  if(timer == 0) {
    for (int i = 0; i < 15; i++) {
    lux--;
    M5.Lcd.setBrightness(lux);  // Appliquer la luminosité maximale
  }
  }
  if (timer < 0) {
    timer = 0;
  }
   if (buttonstatus != lastButtonStatus) {
     lastButtonStatus = buttonstatus;  // Mise à jour de la dernière valeur

    timer = 30000;
    for (int i = 0; i < 255; i++) {
  lux++;
  if(lux > 255) {
    lux = 255;
  }
  M5.Lcd.setBrightness(lux);  // Appliquer la luminosité maximale
  vTaskDelay(6 / portTICK_PERIOD_MS);

}
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Attendre 1 seconde avant la prochaine itération
  }
}
void setup() {
  Serial.begin(115200);
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
      M5.Lcd.setBrightness(255);  // Appliquer la luminosité maximale
  M5Cardputer.Display.setFont(&FreeSans9pt7b);
  M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), YELLOW);
  pixels.setPixelColor(0, pixels.Color(15, 15, 0));
    pixels.show();   // Send the updated pixel colors to the hardware.

  displayMutex = xSemaphoreCreateMutex();

  // Wi-Fi connection
  WiFi.begin();
  Serial.print("Connecting to Wi-Fi");
  M5Cardputer.Display.drawString("Connecting to last WiFi", 4, 4);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");
  M5Cardputer.Display.clear();
  M5Cardputer.Display.drawString("Connected", 4, 4);
    pixels.setPixelColor(0, pixels.Color(0, 15, 0));
    pixels.show();   // Send the updated pixel colors to the hardware.
  xTaskCreate(luxTask, "light", 2048, NULL, 1, NULL);

  M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);

  // Initialize NTP
  timeClient.begin();
  getUserName();
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();   // Send the updated pixel colors to the hardware.

  // Test server connection
  HTTPClient http;
  http.begin(client, serverUrl);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.println("Server connection successful");
    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("SERVER OK", 4, 4);
    inputData = "> ";
    xTaskCreate(
    checkFileTask,          // Fonction de la tâche
    "Check File Task",      // Nom de la tâche
    4096,                   // Taille de la pile
    NULL,                   // Paramètre de la tâche
    1,                      // Priorité de la tâche
    NULL                    // Handle de la tâche (optionnel)
  );

  delay(100);
   // getMessages(); // Fetch and display messages
    M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);
  } else {
    Serial.println("Server connection failed");
    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("SERVER OFFLINE", 4, 4);
    M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), RED);
  }
  http.end();
}

void loop() {
  delay(15);//anti-rebonds
  M5Cardputer.update();

  if (M5Cardputer.BtnA.isPressed()) {
    getMessages(); // Fetch new messages
    autoScrollEnabled = true; // Enable auto-scroll after refreshing
    M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);
  }

  if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB)) {
    delay(300);
    color++;
    if (color >= sizeof(colorMap) / sizeof(colorMap[0])) {
      color = 0;
    }
    updateInputDisplay(); // Update the input prompt with the selected color
  }
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed(';')) {
    if (messageStartIndex > 0) {
      messageStartIndex--; // Défile vers le haut
      autoScrollEnabled = false; // Désactiver l'auto-défilement
      getMessages(); // Met à jour l'affichage des messages
    }
    delay(50); // Anti-rebond
          return; // Évite d'ajouter à la zone de saisie

  }
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed('.')) {
    if (messageStartIndex > 0) {
      messageStartIndex++; // Défile vers le haut
      autoScrollEnabled = false; // Désactiver l'auto-défilement
      getMessages(); // Met à jour l'affichage des messages
    }
    delay(50); // Anti-rebond
          return; // Évite d'ajouter à la zone de saisie

  }
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed(',')) {
  horizontalScrollOffset -= 4; // Déplace vers la gauche
  getMessages(); // Met à jour l'affichage
  delay(50); // Anti-rebond
        return; // Évite d'ajouter à la zone de saisie

}
if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) && M5Cardputer.Keyboard.isKeyPressed('/')) {
  horizontalScrollOffset += 4; // Déplace vers la droite
  getMessages(); // Met à jour l'affichage
  delay(50); // Anti-rebond
        return; // Évite d'ajouter à la zone de saisie

}

  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      buttonstatus++;
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();   // Send the updated pixel colors to the hardware.

      for (auto i : status.word) {
        inputData += i;
      }

      if (status.del && inputData.length() > 2) { // Prevent deleting the "> " prompt
        inputData.remove(inputData.length() - 1);
      }

      if (status.enter) {
        newmessageled = 1;
        String timestamp = getTimestamp();
        sendMessage(inputData.substring(2), timestamp, color); // Send the message with selected color
        inputData = "> ";
        autoScrollEnabled = true; // Enable auto-scroll after sending a message
        getMessages();
        M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);
      }

      updateInputDisplay();
    }
  }
}

void getUserName() {
    username = 1;  // Initialisation pour entrer dans la boucle
    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("Enter a max 6 character", 4, 4);
    M5Cardputer.Display.drawString("username", 4, 18);

    updateInputDisplay();

    while (username == 1) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                    buttonstatus++;
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                // Ajouter les caractères saisis
                for (auto i : status.word) {
                    if (userName.length() < 6) {  // Limiter à 6 caractères
                        userName += i;
                    }
                }

                // Suppression avec la touche DEL
                if (status.del && userName.length() > 0) {
                    userName.remove(userName.length() - 1);
                }

                // Validation avec la touche ENTER
                if (status.enter && userName.length() > 0) {
                    username = 0;  // Quitter la boucle
                }

                // Mise à jour de l'affichage
                M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 28,
                                             M5Cardputer.Display.width(), 25,
                                             BLACK);
                M5Cardputer.Display.setTextColor(WHITE);
                M5Cardputer.Display.drawString("> " + userName, 4, M5Cardputer.Display.height() - 24);
                M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);

            }
            
        }
    }
}

void sendMessage(String message, String timestamp, int colorCode) {
  HTTPClient http;
  http.begin(client, String(serverUrl) + "send_message");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(512);
  doc["message"] = message;
  doc["timestamp"] = timestamp;
  doc["color"] = colorCode; // Include the selected color in the JSON
  String jsonBody;
  serializeJson(doc, jsonBody);

  int httpCode = http.POST(jsonBody);

  if (httpCode > 0) {
    Serial.println("Message sent successfully: " + http.getString());
  } else {
    Serial.println("Error sending the message");
  }

  http.end();
}

void getMessages() {
  HTTPClient http;
  http.begin(client, String(serverUrl) + "get_messages");
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Messages retrieved: " + payload);

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.println("Error parsing messages: " + String(error.c_str()));
      return;
    }
    xSemaphoreTake(displayMutex, portMAX_DELAY);

    // Clear the screen (only the message area)
    M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 28, BLACK);

    JsonArray messages = doc["messages"].as<JsonArray>();
    int maxMessages = (M5Cardputer.Display.height() - 28) / 14;

    // Adjust the visible range based on auto-scroll
    if (autoScrollEnabled) {
      messageStartIndex = std::max(0, static_cast<int>(messages.size()) - maxMessages); // Fix here
    }

    int yOffset = 4;
    for (int i = messageStartIndex; i < messages.size() && i < messageStartIndex + maxMessages; i++) {
      JsonObject message = messages[i];
      String msg = message["message"].as<String>();
      String timestamp = message["timestamp"].as<String>();
      int colorCode = message["color"].as<int>(); // Retrieve color for each message
      ////////////////////////////////////////////////////////////////////////
      //String displayLine = timestamp + ": " + msg;
    //  M5Cardputer.Display.setTextColor(colorMap[colorCode]); // Set text color for the message
     // M5Cardputer.Display.drawString(displayLine, 4 - horizontalScrollOffset, yOffset);
     // yOffset += 14;
      //////////////////////////////////////////////////////////////////////////
      String displayLine = timestamp + ": " + msg;
String wrappedLine = wrapText(displayLine, 30);
M5Cardputer.Display.setTextColor(colorMap[colorCode]);

int lineYOffset = yOffset;
for (int j = 0; j < wrappedLine.length(); j++) {
  String line = wrappedLine.substring(j, wrappedLine.indexOf('\n', j));
  M5Cardputer.Display.drawString(line, 4 - horizontalScrollOffset, lineYOffset);
  lineYOffset += 14;
  j = wrappedLine.indexOf('\n', j);
  if (j == -1) break;
}

yOffset = lineYOffset;
//////////////////////////////////////////////////////////////////////////////////////////////
    }
              M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);

        xSemaphoreGive(displayMutex);  // Libérer le mutex après l'affichage

  } else {
    Serial.println("Error retrieving messages");
    M5Cardputer.Display.drawString("Error retrieving messages", 4, 4);
    M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), RED);
  }
  http.end();
}


String wrapText(String text, int maxChars) {
  String wrappedText = "";
  for (int i = 0; i < text.length(); i++) {
    wrappedText += text.charAt(i);
    if ((i + 1) % maxChars == 0) {
      wrappedText += "\n";
    }
  }
  return wrappedText;
}



String getTimestamp() {
  // Retourner le nom de l'utilisateur comme timestamp
  return userName;
}

void updateInputDisplay() {
    xSemaphoreTake(displayMutex, portMAX_DELAY);  // Prendre le mutex avant d'afficher

  // Clear the input area
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 28, M5Cardputer.Display.width(), 28, BLACK);

  // Display the prompt with the selected color
  M5Cardputer.Display.setTextColor(colorMap[color]);
  M5Cardputer.Display.drawString(inputData, 4, M5Cardputer.Display.height() - 24);

  // Reset color to default to avoid affecting other areas
  M5Cardputer.Display.setTextColor(WHITE);
          M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(), M5Cardputer.Display.height(), GREEN);
            xSemaphoreGive(displayMutex);  // Libérer le mutex après l'affichage


}
void checkFileTask(void* parameter) {
  const char* messageUrl = "http://176.174.44.98/messages/messages.json"; // Server address

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(messageUrl); // Initialiser la requête HTTP

      int httpResponseCode = http.GET(); // Effectuer une requête GET

      if (httpResponseCode == 200) { // Si la requête est réussie
        String fileContent = http.getString();

        if (fileContent != lastFileContent) {
          Serial.println("file changed");
          Serial.println("message.json: ");
          Serial.println(fileContent);
          if(newmessageled == 1) {
            newmessageled = 0;
            pixels.setPixelColor(0, pixels.Color(0, 0, 0));
                pixels.show();   // Send the updated pixel colors to the hardware.

          }else{
            buttonstatus++;
            pixels.setPixelColor(0, pixels.Color(0, 0, 255));
                pixels.show();   // Send the updated pixel colors to the hardware.

          }
          getMessages();
          // Sauvegarder le nouveau contenu
          lastFileContent = fileContent;

          // Décoder le contenu JSON
          DynamicJsonDocument doc(8192);  // Utiliser DynamicJsonDocument pour plus de flexibilité
          DeserializationError error = deserializeJson(doc, fileContent);
          if (error) {
            Serial.println("Erreur de décodage JSON :");
            Serial.println(error.c_str());
          } else {
            Serial.println("Contenu JSON valide :");
            serializeJsonPretty(doc, Serial);
            Serial.println();
          }
        } else {
          Serial.println("Aucun changement détecté.");
        }
      } else {
        Serial.print("Erreur HTTP : ");
        Serial.println(httpResponseCode);
      }

      http.end(); // Fermer la connexion HTTP
    } else {
      Serial.println("Wi-Fi déconnecté !");
    }
    // Attendre 5 secondes avant la prochaine vérification
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
