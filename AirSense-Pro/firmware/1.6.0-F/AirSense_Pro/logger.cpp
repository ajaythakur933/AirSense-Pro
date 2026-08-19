#include "logger.h"

static WiFiServer logServer(AIRSENSE_TCP_LOG_PORT);
static WiFiClient logClient;
static bool logServerStarted = false;

static uint32_t tcpConnectionCount = 0;
static uint32_t tcpDisconnectCount = 0;
static unsigned long lastTcpHealthCheck = 0;
static const unsigned long TCP_HEALTH_CHECK_INTERVAL_MS = 5000UL;

static void closeLogClient(const char* reason)
{
  if (logClient) {
    logClient.stop();
  }

  if (reason) {
    Serial.print("[TCP LOG] ");
    Serial.println(reason);
  }

  tcpDisconnectCount++;
}

static bool sendToClient(const char* data)
{
  if (!logClient || !logClient.connected()) {
    return false;
  }

  size_t len = strlen(data);
  size_t written = logClient.write((const uint8_t*)data, len);

  if (written != len) {
    closeLogClient("Client disconnected / write failed");
    return false;
  }

  return true;
}

static bool sendToClient(const String &data)
{
  if (!logClient || !logClient.connected()) {
    return false;
  }

  size_t written = logClient.print(data);

  if (written != data.length()) {
    closeLogClient("Client disconnected / write failed");
    return false;
  }

  return true;
}

void loggerBegin() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!logServerStarted) {
    logServer.begin();
    logServer.setNoDelay(true);
    logServerStarted = true;

    Serial.printf("[TCP LOG] Server started on port %u\n",
                  AIRSENSE_TCP_LOG_PORT);
    Serial.println("[TCP LOG] Waiting for client...");
  }
}

void loggerLoop() {
  if (!logServerStarted) {
    if (WiFi.status() == WL_CONNECTED) {
      loggerBegin();
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (logClient) {
      closeLogClient("Wi-Fi offline - client closed");
    }
    return;
  }

  // If the previous client has gone away, release it before
  // accepting a new client. The explicit stop() is important
  // for repeated PuTTY connect/disconnect cycles.
  if (logClient && !logClient.connected()) {
    closeLogClient("Client disconnected");
  }

  // Accept at most one client.
  if (!logClient) {
    WiFiClient incoming = logServer.accept();

    if (incoming) {
      incoming.setNoDelay(true);
      logClient = incoming;

      tcpConnectionCount++;

      Serial.println("[TCP LOG] Client connected");
      Serial.print("[TCP LOG] Client connections: ");
      Serial.println(tcpConnectionCount);

      if (!sendToClient("AirSense Pro TCP Debug Log\r\n") ||
          !sendToClient("Firmware TCP log port: 2323\r\n") ||
          !sendToClient("================================\r\n")) {
        return;
      }

      sendToClient("[TCP LOG] Live logging enabled\r\n");
      lastTcpHealthCheck = millis();
    }
  }

  // A periodic one-byte newline acts as a lightweight health check.
  // It also gives us an opportunity to detect a dead TCP socket
  // even when application logging is temporarily quiet.
  if (logClient &&
      logClient.connected() &&
      millis() - lastTcpHealthCheck >= TCP_HEALTH_CHECK_INTERVAL_MS) {

    lastTcpHealthCheck = millis();

    if (!sendToClient("\r\n")) {
      // sendToClient() already closed and reported the client.
      return;
    }
  }
}

void loggerPrint(const String &msg) {
  Serial.print(msg);

  if (logClient && logClient.connected()) {
    sendToClient(msg);
  }
}

void loggerPrintln(const String &msg) {
  Serial.println(msg);

  if (logClient && logClient.connected()) {
    String line = msg;
    line += "\r\n";
    sendToClient(line);
  }
}

void loggerPrintln(const char *msg) {
  Serial.println(msg);

  if (logClient && logClient.connected()) {
    String line(msg);
    line += "\r\n";
    sendToClient(line);
  }
}

bool loggerClientConnected() {
  return logClient && logClient.connected();
}

uint32_t loggerGetConnectionCount() {
  return tcpConnectionCount;
}

uint32_t loggerGetDisconnectCount() {
  return tcpDisconnectCount;
}
