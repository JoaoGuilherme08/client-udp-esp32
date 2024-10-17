#include "WiFi.h"
#include "AsyncUDP.h"

const char *ssid = "RTK-2.4G";
const char *password = "joaogabi1210";

AsyncUDP udp;

// Parâmetros de Diffie-Hellman
const unsigned long g = 5; // Gerador
const unsigned long p = 23; // Número primo
unsigned long privateKey;
unsigned long publicKey;
unsigned long sharedKey;
bool keyExchangeComplete = false;
unsigned long lastSendTime = 0;
unsigned long TIMEOUT = 10000; // 10 segundos para timeout

// Função para calcular a exponenciação modular
unsigned long modExp(unsigned long base, unsigned long exp, unsigned long mod) {
  unsigned long result = 1;
  while (exp > 0) {
    if (exp % 2 == 1) {
      result = (result * base) % mod;
    }
    base = (base * base) % mod;
    exp = exp / 2;
  }
  return result;
}

// Função para criptografar a mensagem com XOR usando a chave compartilhada
String encryptMessage(String message, unsigned long key) {
  String encryptedMessage = "";
  for (int i = 0; i < message.length(); i++) {
    encryptedMessage += (char)(message[i] ^ (key & 0xFF));  // XOR com o último byte da chave
  }
  return encryptedMessage;
}

void generateKeys() {
  privateKey = random(1, p-1);
  publicKey = modExp(g, privateKey, p);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed");
    while (1) {
      delay(1000);
    }
  }

  // Gerar par de chaves Diffie-Hellman
  generateKeys();
  Serial.println("Chave pública do ESP32 gerada");

  if (udp.connect(IPAddress(192, 168, 1, 227), 1234)) {
    Serial.println("UDP connected");

    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.print("Recebido pacote UDP de: ");
      Serial.print(packet.remoteIP());
      Serial.print(":");
      Serial.print(packet.remotePort());
      Serial.print(", Dados: ");
      Serial.write(packet.data(), packet.length());
      Serial.println();

      String dataReceived = String((char*)packet.data());  
      if (dataReceived.equals("RTC")) {  
                Serial.println("Mensagem de reconexão recebida, reiniciando a troca de chaves...");
                keyExchangeComplete = false;
                generateKeys();
                char publicKeyStr[20];
                itoa(publicKey, publicKeyStr, 10);
                udp.print(publicKeyStr);
                Serial.print("Chave pública reenviada: ");
                Serial.println(publicKeyStr);
      }else if (!keyExchangeComplete) {
        // Receber chave pública do servidor e calcular chave compartilhada
          unsigned long serverPublicKey = strtoul(dataReceived.c_str(), NULL, 10);
          sharedKey = modExp(serverPublicKey, privateKey, p);
          keyExchangeComplete = true;
          Serial.println("Chave compartilhada gerada com sucesso!");
      } else {
        // Se a chave já foi trocada, processar a mensagem normalmente
        Serial.println("Mensagem recebida após troca de chaves.");
      }
    });

    // Enviar a chave pública do ESP32 para o servidor
    char publicKeyStr[20];
    itoa(publicKey, publicKeyStr, 10);
    udp.print(publicKeyStr);
    Serial.print("Chave pública enviada: ");
    Serial.println(publicKeyStr);
  }
}

void loop() {
  unsigned long currentTime = millis();

  if (keyExchangeComplete) {
    if (currentTime - lastSendTime >= 1000) {  // Enviar mensagem a cada segundo
      String message = "Hello Server!";
      String encryptedMessage = encryptMessage(message, sharedKey);
      udp.print(encryptedMessage.c_str());
      Serial.print("Mensagem enviada: ");
      Serial.println(encryptedMessage);
      lastSendTime = currentTime;
    }
  } else {
    Serial.println("Aguardando a conclusão da troca de chaves...");
    if (currentTime - lastSendTime >= TIMEOUT) {
      // Se o servidor não respondeu em 10 segundos, reiniciar a troca de chaves
      generateKeys();
      char publicKeyStr[20];
      itoa(publicKey, publicKeyStr, 10);
      udp.print(publicKeyStr);
      Serial.println("Reiniciando a troca de chaves...");
      lastSendTime = currentTime;
    }
  }
}
