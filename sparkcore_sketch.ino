/*
Eric Tsai
2014-11-24
Connected mousetrap publishes MQTT messages to Openhab
http://www.instructables.com/id/Better-Smarter-Mousetrap/

*/

//-----------------------------------------------------------------------------------
// This #include statement was automatically added by the Spark IDE.
#include "Adafruit_DHT/Adafruit_DHT.h"

/*
  PubSubClient.cpp - A simple client for MQTT.
  Original Code - Nicholas O'Leary
  http://knolleary.net

  Adapted for Spark Core by Chris Howard - chris@kitard.com
  Based on PubSubClient 1.9.1

  Changes
  - Added gcc pragma to avoid warnings being thrown as errors (deprecated conversion from string constant to 'char*')
  - publish_P function removed due to lack of Arduino PROGMEN support on the Spark Core
  - Obvious includes commented out
  - Using Spark TCPClient instead of Arduino EthernetClient

*/

#pragma GCC diagnostic ignored "-Wwrite-strings"

// #include "PubSubClient.h"
// #include <Arduino.h>
#define ARDUINO_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

//#include "Client.h"

// MQTT_MAX_PACKET_SIZE : Maximum packet size
#define MQTT_MAX_PACKET_SIZE 128

// MQTT_KEEPALIVE : keepAlive interval in Seconds
#define MQTT_KEEPALIVE 15

#define MQTTPROTOCOLVERSION 3
#define MQTTCONNECT     1 << 4  // Client request to connect to Server
#define MQTTCONNACK     2 << 4  // Connect Acknowledgment
#define MQTTPUBLISH     3 << 4  // Publish message
#define MQTTPUBACK      4 << 4  // Publish Acknowledgment
#define MQTTPUBREC      5 << 4  // Publish Received (assured delivery part 1)
#define MQTTPUBREL      6 << 4  // Publish Release (assured delivery part 2)
#define MQTTPUBCOMP     7 << 4  // Publish Complete (assured delivery part 3)
#define MQTTSUBSCRIBE   8 << 4  // Client Subscribe request
#define MQTTSUBACK      9 << 4  // Subscribe Acknowledgment
#define MQTTUNSUBSCRIBE 10 << 4 // Client Unsubscribe request
#define MQTTUNSUBACK    11 << 4 // Unsubscribe Acknowledgment
#define MQTTPINGREQ     12 << 4 // PING Request
#define MQTTPINGRESP    13 << 4 // PING Response
#define MQTTDISCONNECT  14 << 4 // Client is Disconnecting
#define MQTTReserved    15 << 4 // Reserved

#define MQTTQOS0        (0 << 1)
#define MQTTQOS1        (1 << 1)
#define MQTTQOS2        (2 << 1)

class PubSubClient {
private:
   //Client* _client;
   TCPClient* _client; // CH 14Jan2014 - changed Client* to TCPClient*
   uint8_t buffer[MQTT_MAX_PACKET_SIZE];
   uint16_t nextMsgId;
   unsigned long lastOutActivity;
   unsigned long lastInActivity;
   bool pingOutstanding;
   void (*callback)(char*,uint8_t*,unsigned int);
   uint16_t readPacket(uint8_t*);
   uint8_t readByte();
   bool write(uint8_t header, uint8_t* buf, uint16_t length);
   uint16_t writeString(char* string, uint8_t* buf, uint16_t pos);
   uint8_t *ip;
   char* domain;
   uint16_t port;
public:
   PubSubClient();
   PubSubClient(uint8_t *, uint16_t, void(*)(char*,uint8_t*,unsigned int),TCPClient& client); // CH 14Jan2014 - changed Client& to TCPClient&
   PubSubClient(char *, uint16_t, void(*)(char *,uint8_t*,unsigned int),TCPClient&  client); // CH 14Jan2014 - changed Client& to TCPClient&
   //bool connect(const char *);
   bool connect(char *);
   bool connect(char *, char *, char *);
   bool connect(char *, char *, uint8_t, uint8_t, char *);
   bool connect(char *, char *, char *, char *, uint8_t, uint8_t, char *);
   void disconnect();
   bool publish(char *, char *);
   bool publish(char *, uint8_t *, unsigned int);
   bool publish(char *, uint8_t *, unsigned int, bool);
   bool subscribe(char *);
   bool subscribe(char *, uint8_t qos);
   bool unsubscribe(char *);
   bool puback(uint16_t msgId);
   bool loop();
   bool connected();
};


#include <string.h>

PubSubClient::PubSubClient() {
   this->_client = NULL;
}

PubSubClient::PubSubClient(uint8_t *ip, uint16_t port, void (*callback)(char*,uint8_t*,unsigned int), TCPClient& client) { // CH 14Jan2014 - Changed Client& to TCPClient&
   this->_client = &client;
   this->callback = callback;
   this->ip = ip;
   this->port = port;
   this->domain = NULL;
}

PubSubClient::PubSubClient(char* domain, uint16_t port, void (*callback)(char*,uint8_t*,unsigned int), TCPClient& client) { // CH 14Jan2014 - Changed Client& to TCPClient&
   this->_client = &client;
   this->callback = callback;
   this->domain = domain;
   this->port = port;
}


// CONNECT

//bool PubSubClient::connect(const char *id) {
bool PubSubClient::connect(char *id) {
   return connect(id,NULL,NULL,0,0,0,0);
}

bool PubSubClient::connect(char *id, char *user, char *pass) {
   return connect(id,user,pass,0,0,0,0);
}

bool PubSubClient::connect(char *id, char* willTopic, uint8_t willQos, uint8_t willRetain, char* willMessage)
{
   return connect(id,NULL,NULL,willTopic,willQos,willRetain,willMessage);
}

bool PubSubClient::connect(char *id, char *user, char *pass, char* willTopic, uint8_t willQos, uint8_t willRetain, char* willMessage) {
   if (!connected()) {
      int result = 0;

      if (domain != NULL) {
        result = _client->connect(this->domain, this->port);
      } else {
        result = _client->connect(this->ip, this->port);
      }

      if (result) {
         nextMsgId = 1;
         uint8_t d[9] = {0x00,0x06,'M','Q','I','s','d','p',MQTTPROTOCOLVERSION};
         // Leave room in the buffer for header and variable length field
         uint16_t length = 5;
         unsigned int j;
         for (j = 0;j<9;j++) {
            buffer[length++] = d[j];
         }

         uint8_t v;
         if (willTopic) {
            v = 0x06|(willQos<<3)|(willRetain<<5);
         } else {
            v = 0x02;
         }

         if(user != NULL) {
            v = v|0x80;

            if(pass != NULL) {
               v = v|(0x80>>1);
            }
         }

         buffer[length++] = v;

         buffer[length++] = ((MQTT_KEEPALIVE) >> 8);
         buffer[length++] = ((MQTT_KEEPALIVE) & 0xFF);
         length = writeString(id,buffer,length);
         if (willTopic) {
            length = writeString(willTopic,buffer,length);
            length = writeString(willMessage,buffer,length);
         }

         if(user != NULL) {
            length = writeString(user,buffer,length);
            if(pass != NULL) {
               length = writeString(pass,buffer,length);
            }
         }

         write(MQTTCONNECT,buffer,length-5);

         lastInActivity = lastOutActivity = millis();

         while (!_client->available()) {
            unsigned long t = millis();
            if (t-lastInActivity > MQTT_KEEPALIVE*1000UL) {
               _client->stop();
               return false;
            }
         }
         uint8_t llen;
         uint16_t len = readPacket(&llen);

         if (len == 4 && buffer[3] == 0) {
            lastInActivity = millis();
            pingOutstanding = false;
            return true;
         }
      }
      _client->stop();
   }
   return false;
}

uint8_t PubSubClient::readByte() {
   while(!_client->available()) {}
   return _client->read();
}

uint16_t PubSubClient::readPacket(uint8_t* lengthLength) {
   uint16_t len = 0;
   buffer[len++] = readByte();
   uint32_t multiplier = 1;
   uint16_t length = 0;
   uint8_t digit = 0;
   do {
      digit = readByte();
      buffer[len++] = digit;
      length += (digit & 127) * multiplier;
      multiplier *= 128;
   } while ((digit & 128) != 0);
   *lengthLength = len-1;
   for (uint16_t i = 0;i<length;i++)
   {
      if (len < MQTT_MAX_PACKET_SIZE) {
         buffer[len++] = readByte();
      } else {
         readByte();
         len = 0; // This will cause the packet to be ignored.
      }
   }

   return len;
}

bool PubSubClient::loop() {
   if (connected()) {
      unsigned long t = millis();
      if ((t - lastInActivity > MQTT_KEEPALIVE*1000UL) || (t - lastOutActivity > MQTT_KEEPALIVE*1000UL)) {
         if (pingOutstanding) {
            _client->stop();
            return false;
         } else {
            buffer[0] = MQTTPINGREQ;
            buffer[1] = 0;
            _client->write(buffer,2);
            lastOutActivity = t;
            lastInActivity = t;
            pingOutstanding = true;
         }
      }
      if (_client->available()) {
         uint8_t llen;
         uint16_t len = readPacket(&llen);
         uint16_t msgId = 0;
         uint8_t *payload;
         if (len > 0) {
            lastInActivity = t;
            uint8_t type = buffer[0]&0xF0;
            if (type == MQTTPUBLISH) {
               if (callback) {
                  uint16_t tl = (buffer[llen+1]<<8)+buffer[llen+2];
                  char topic[tl+1];
                  for (uint16_t i=0;i<tl;i++) {
                     topic[i] = buffer[llen+3+i];
                  }
                  topic[tl] = 0;
                  // msgId only present for QOS>0
                  if (buffer[0]&MQTTQOS1) {
                    msgId = (buffer[llen+3+tl]<<8)+buffer[llen+3+tl+1];
                    payload = buffer+llen+3+tl+2;
                    callback(topic,payload,len-llen-3-tl-2);
                    puback(msgId);
                  } else {
                    payload = buffer+llen+3+tl;
                    callback(topic,payload,len-llen-3-tl);
                  }
               }
            } else if (type == MQTTPINGREQ) {
               buffer[0] = MQTTPINGRESP;
               buffer[1] = 0;
               _client->write(buffer,2);
            } else if (type == MQTTPINGRESP) {
               pingOutstanding = false;
            }
         }
      }
      return true;
   }
   return false;
}


// PUBLISH

bool PubSubClient::publish(char* topic, char* payload) {
   return publish(topic,(uint8_t*)payload,strlen(payload),false);
}

bool PubSubClient::publish(char* topic, uint8_t* payload, unsigned int plength) {
   return publish(topic, payload, plength, false);
}

bool PubSubClient::publish(char* topic, uint8_t* payload, unsigned int plength, bool retained) {
   if (connected()) {
      // Leave room in the buffer for header and variable length field
      uint16_t length = 5;
      length = writeString(topic,buffer,length);
      uint16_t i;
      for (i=0;i<plength;i++) {
         buffer[length++] = payload[i];
      }
      uint8_t header = MQTTPUBLISH;
      if (retained) {
         header |= 1;
      }
      return write(header,buffer,length-5);
   }
   return false;
}

bool PubSubClient::write(uint8_t header, uint8_t* buf, uint16_t length) {
   uint8_t lenBuf[4];
   uint8_t llen = 0;
   uint8_t digit;
   uint8_t pos = 0;
   uint8_t rc;
   uint8_t len = length;
   do {
      digit = len % 128;
      len = len / 128;
      if (len > 0) {
         digit |= 0x80;
      }
      lenBuf[pos++] = digit;
      llen++;
   } while(len>0);

   buf[4-llen] = header;
   for (int i=0;i<llen;i++) {
      buf[5-llen+i] = lenBuf[i];
   }
   rc = _client->write(buf+(4-llen),length+1+llen);

   lastOutActivity = millis();
   return (rc == 1+llen+length);
}

bool PubSubClient::subscribe(char* topic) {
  return subscribe(topic, 0);
}

// SUBSCRIBE

bool PubSubClient::subscribe(char* topic, uint8_t qos) {
   if (qos < 0 || qos > 1)
     return false;

   if (connected()) {
      // Leave room in the buffer for header and variable length field
      uint16_t length = 5;
      nextMsgId++;
      if (nextMsgId == 0) {
         nextMsgId = 1;
      }
      buffer[length++] = (nextMsgId >> 8);
      buffer[length++] = (nextMsgId & 0xFF);
      length = writeString(topic, buffer,length);
      buffer[length++] = qos;
      return write(MQTTSUBSCRIBE|MQTTQOS1,buffer,length-5);
   }
   return false;
}

bool PubSubClient::puback(uint16_t msgId) {
  if(connected()) {
    // Leave room in the buffer for header and variable length field
    uint16_t length = 5;
    buffer[length++] = (msgId >> 8);
    buffer[length++] = (msgId & 0xFF);
    return write(MQTTPUBACK,buffer,length-5);
  }
  return false;
}

// HELPERS

//bool PubSubClient::unsubscribe(char* topic) {
bool PubSubClient::unsubscribe(char* topic) {
   if (connected()) {
      uint16_t length = 5;
      nextMsgId++;
      if (nextMsgId == 0) {
         nextMsgId = 1;
      }
      buffer[length++] = (nextMsgId >> 8);
      buffer[length++] = (nextMsgId & 0xFF);
      length = writeString(topic, buffer,length);
      return write(MQTTUNSUBSCRIBE|MQTTQOS1,buffer,length-5);
   }
   return false;
}

void PubSubClient::disconnect() {
   buffer[0] = MQTTDISCONNECT;
   buffer[1] = 0;
   _client->write(buffer,2);
   _client->stop();
   lastInActivity = lastOutActivity = millis();
}

uint16_t PubSubClient::writeString(char* string, uint8_t* buf, uint16_t pos) {
   char* idp = string;
   uint16_t i = 0;
   pos += 2;
   while (*idp) {
      buf[pos++] = *idp++;
      i++;
   }
   buf[pos-i-2] = (i >> 8);
   buf[pos-i-1] = (i & 0xFF);
   return pos;
}


bool PubSubClient::connected() {
   bool rc;
   if (_client == NULL ) {
      rc = false;
   } else {
      rc = (int)_client->connected();
      if (!rc) _client->stop();
   }
   return rc;
}


// MAIN APPLICATOIN CODE STARTS HERE
// Eric Tsai
// 11/23/2014 - Smart Mouse Trap
// 1.  Update MQTT server "ip[]"

byte ip[] = { 192, 168, 1, 9 };     //your MQTT server
int LED = D4; // pin for LED
int PIR = D3; //pin for PIR motion sensor
//place tcpClient here for call back
TCPClient tcpClient;
PubSubClient client(ip, 1883, callback, tcpClient);
unsigned long temp_sensor_time=0;

Servo myservo;  //declare servo object
int pos = 10;    // variable to store the servo position 

bool status_captured = 0;  //1 = captured mouse, 0=no mouse
bool mode_vacation= 0;  //1 = vacation mode, close shutters
bool mode_lonely = 1; //1 = one mouse at a time
bool publish_flag_capture = 0;

unsigned long temp_time = 0;  //also serves as heart beat

//DHT11 setup
DHT mydht(2, DHT11);

void callback(char* topic, byte* payload, unsigned int length) 
{
  // handle message arrived - we are only subscribing to one topic so assume all are led related

    char buff_topic[6];
    char buff_message[12];  
    
    int mytopic = atoi (topic); //convert topic into integer.  Assume all topics are 4 digit ints

    //something wrong with atol, atoi, atof calls.  Work around.
    payload[length] = '\0';
    char* cstring = (char *) payload;
    long msg = atol (cstring);
    //float msg = atof( (const char *) payload);    //does not work for me


    if (mytopic == 9999)        //vacation mode on
    {
        mode_vacation = 1;
    }//end if mytopic

    if (mytopic == 9998)        //vacation mode off
    {
        mode_vacation = 0;
    }//end if mytopic

    if (mytopic == 9997)        //clear capture status
    {
        publish_flag_capture = 0;
    }//end if mytopic
    
    if (mytopic == 9996)    //lonely mode on
    {
        mode_lonely = 1;
    }//end if mytopic
    
    if (mytopic == 9995)    //lonely mode off
    {
        mode_lonely = 0;
    }//end if mytopic
    
}//end function callback



// Simple MQTT demo to allow the blue led (D7) to be turned on or off. Send message to topic "led" with paylod aoff "on" or "off"

void setup()
{
    //Serial.begin(9600);
    pinMode(LED, OUTPUT); // Use for a simple test of the led on or off by subscribing to a topical called led
    pinMode(PIR, INPUT);
  //if (client.connect("Spark")) { // Anonymous authentication enabled
  if (client.connect("spark", "userid", "password")) { // uid:pwd based authentication
    client.publish("Spark Status","I'm Alive...");
    
    //basement trap
    

    client.subscribe("9999");   //vacation mode on
    delay(3000);
    
    client.subscribe("9998");   //vacation mode off
    delay(3000);
    
    client.subscribe("9997");   //clear capture status
    delay(3000);
    
	//too many subscriptions made unstable MQTT connection?  Spark core library issue?
    //client.subscribe("9996");   //lonely mode on
    //delay(3000);
    
    //client.subscribe("9995");   //lonely mode off
    //delay(3000);
    
  }
  
  //servo setup and initialize
  myservo.attach(D0);
  myservo.write(170);
  
  //blink light
  digitalWrite(LED, HIGH);
  delay(1000);
  digitalWrite(LED, LOW);
  
  //reset trap on openhab display
  client.publish("mouse_basement_trap_reset","1");
  publish_flag_capture = 0;
}


unsigned long PIR_timer = 0;   

void loop()
{
    client.loop();
  
    int seeMotion = digitalRead(PIR);
    if ((seeMotion == 1) && (publish_flag_capture != 1))
    {
        PIR_timer = millis();
        publish_flag_capture = 1;
        client.publish("mouse_basement_trap_mouse","1");
    }

    
    if (publish_flag_capture == 1)
    {
        if ((millis()%1000) > 500 )
        {
            digitalWrite(LED, LOW);
        }
        else
            digitalWrite(LED, HIGH);
    }
    else
    {
        digitalWrite(LED, LOW);
    }
    
    
    //move servo motor
    //close servo if trap set or vacation
    //open servo if trap reset or vacation not set
    
    if ((mode_vacation == 1) || ((publish_flag_capture == 1) && (mode_lonely == 1))) 
    {
        myservo.write(10);  //close servo door
    }
    else
    {
        myservo.write(95);    //open servo door
    }
    
  
  //send temperature regularly
  if ((millis() - temp_sensor_time) > 130000)
  {
    //client.publish("11122","I'm Alive. yo yo");
    float h = mydht.getHumidity();
    float t = mydht.getTempCelcius();
    char debug_value[4];

    t = t * 9/5 + 32;

    char temperature_str[12];
    sprintf(temperature_str, "%4.1f", t);
    client.publish("mouse_basement_temp",temperature_str);
    
    temp_sensor_time = millis();
  } //if millis
} //end loop