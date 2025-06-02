#ifndef MOCK_WEBSERVER_H
#define MOCK_WEBSERVER_H

#include <string>

// Mock webserver functions
void handleClient(class MockClient& client);
void sendHttpResponse(class MockClient& client, int statusCode, const char* contentType, const char* content);

#endif // MOCK_WEBSERVER_H 