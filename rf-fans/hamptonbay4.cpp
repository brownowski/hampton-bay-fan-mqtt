#include "rf-fans.h"


#define BASE_TOPIC HAMPTONBAY4_BASE_TOPIC

#define CMND_BASE_TOPIC CMND_TOPIC BASE_TOPIC
#define STAT_BASE_TOPIC STAT_TOPIC BASE_TOPIC

#define SUBSCRIBE_TOPIC_CMND CMND_BASE_TOPIC "/#"

#define SUBSCRIBE_TOPIC_STAT_SETUP STAT_BASE_TOPIC "/#"

#ifndef HAMPTONBAY4_TX_FREQ
  #define TX_FREQ 304.7 // UC7078TR Hampton Bay made by Chia Wei Electric
#else
  #define TX_FREQ HAMPTONBAY4_TX_FREQ
#endif

// RC-switch settings
#define RF_PROTOCOL 15
#define RF_REPEATS  12
                                    
// Keep track of states for all dip settings
static fan fans[16];

static int long lastvalue;
static unsigned long lasttime;

static void postStateUpdate(int id) {
  sprintf(outTopic, "%s/%s/direction", STAT_BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fans[id].directionState ? "REVERSE":"FORWARD", true);
  sprintf(outTopic, "%s/%s/fan", STAT_BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fans[id].fanState ? "ON":"OFF", true);
  sprintf(outTopic, "%s/%s/speed", STAT_BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fanStateTable[fans[id].fanSpeed], true);
  sprintf(outTopic, "%s/%s/light", STAT_BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fans[id].lightState ? "ON":"OFF", true);

  sprintf(outTopic, "%s/%s/percent", STAT_BASE_TOPIC, idStrings[id]);
  *outPercent='\0';
  if(fans[id].fanState) {
    switch(fans[id].fanSpeed) {
      case FAN_HI:
        sprintf(outPercent,"%d",FAN_PCT_HI);
        break;
      case FAN_MED:
        sprintf(outPercent,"%d",FAN_PCT_MED);
        break;
      case FAN_LOW:
        sprintf(outPercent,"%d",FAN_PCT_LOW);
        break;
    }
  } else
    sprintf(outPercent,"%d",FAN_PCT_OFF);
  client.publish(outTopic, outPercent, true);
}

static void transmitState(int fanId, int code) {
  mySwitch.disableReceive();         // Receiver off
  ELECHOUSE_cc1101.setMHZ(TX_FREQ);
  ELECHOUSE_cc1101.SetTx();           // set Transmit on
  mySwitch.enableTransmit(TX_PIN);   // Transmit on
  mySwitch.setRepeatTransmit(RF_REPEATS); // transmission repetitions.
  mySwitch.setProtocol(RF_PROTOCOL);        // send Received Protocol

// Build out RF code
  //   Code follows the 12 bit pattern, built ontop of harberbreeze?
  //   0aaaaxxxxxxx
  //   Where a is the inversed/reversed dip setting, 
  //   And xxxxxxxx is the code below
  // 1011111  H	
  // 1101111  M	
  // 1110111  L
  // 1111101  Off
  // Harber Breeze UC7000T

  int rfCode = (((~fanId) &0x0f) << 7) | ((code&0x7f));
  
  mySwitch.send(rfCode, 12);      // send 24 bit code
  mySwitch.disableTransmit();   // set Transmit off
  ELECHOUSE_cc1101.setMHZ(RX_FREQ);
  ELECHOUSE_cc1101.SetRx();      // set Receive on
  mySwitch.enableReceive(RX_PIN);   // Receiver on
  Serial.print("Sent command hamptonbay4: ");
  Serial.print(fanId);
  Serial.print(" ");
  for(int b=24; b>0; b--) {
    Serial.print(bitRead(rfCode,b-1));
  }
  Serial.println("");
  postStateUpdate(fanId);
}

void hamptonbay4MQTT(char* topic, char* payloadChar, unsigned int length) {
  if(strncmp(topic, CMND_BASE_TOPIC, sizeof(CMND_BASE_TOPIC)-1) == 0) {
  
    // Get ID after the base topic + a slash
    char id[5];
    int percent;
    memcpy(id, &topic[sizeof(CMND_BASE_TOPIC)], 4);
    id[4] = '\0';
    if(strspn(id, idchars)) {
      uint8_t idint = strtol(id, (char**) NULL, 2);
      char *attr;
      // Split by slash after ID in topic to get attribute and action
    
      attr = strtok(topic+sizeof(CMND_BASE_TOPIC)-1 + 5, "/");

      if(strcmp(attr,"percent") ==0) {
        percent=atoi(payloadChar);
        if(percent > FAN_PCT_OVER) {
          fans[idint].fanState = true;
          if(percent > (FAN_PCT_MED + FAN_PCT_OVER)) {
            fans[idint].fanSpeed=FAN_HI;
            transmitState(idint,0xfe);
          } else if(percent > (FAN_PCT_LOW + FAN_PCT_OVER)) {
            fans[idint].fanSpeed=FAN_MED;
            transmitState(idint,0xfd);
          } else {
            fans[idint].fanSpeed=FAN_LOW;
            transmitState(idint,0xbf);
          }
        } else {
          fans[idint].fanState = false;
          transmitState(idint,0xdf);
        }
      } else if(strcmp(attr,"fan") ==0) {
        if(strcmp(payloadChar,"toggle") == 0) {
          if(fans[idint].fanState)
            strcpy(payloadChar,"off");
          else
            strcpy(payloadChar,"on");
        }
        if(strcmp(payloadChar,"on") == 0) {
          fans[idint].fanState = true;
          switch(fans[idint].fanSpeed) {
            case FAN_HI:
              transmitState(idint,HAMPTONBAY4_CODE_HI);
              break;
            case FAN_MED:
              transmitState(idint,HAMPTONBAY4_CODE_MED);
              break;
            case FAN_LOW:
              transmitState(idint,HAMPTONBAY4_CODE_LOW);
              break;
          }
        } else {
          fans[idint].fanState = false;
          transmitState(idint,HAMPTONBAY4_CODE_OFF);
        }
      } else if(strcmp(attr,"speed") ==0) {
        if(strcmp(payloadChar,"+") ==0) {
          fans[idint].fanState = true;
          switch(fans[idint].fanSpeed) {
            case FAN_LOW:
              fans[idint].fanSpeed=FAN_MED;
              transmitState(idint,HAMPTONBAY4_CODE_MED);
              break;
            case FAN_MED:
              fans[idint].fanSpeed=FAN_HI;
              transmitState(idint,HAMPTONBAY4_CODE_HI);
              break;
            case FAN_HI:
              fans[idint].fanSpeed=FAN_HI;
              transmitState(idint,HAMPTONBAY4_CODE_HI);
              break;
            default:
              if(fans[idint].fanSpeed>FAN_HI)
                fans[idint].fanSpeed--;
              break;
          }
        } else if(strcmp(payloadChar,"-") ==0) {
          fans[idint].fanState = true;
          switch(fans[idint].fanSpeed) {
            case FAN_HI:
              fans[idint].fanSpeed=FAN_MED;
              transmitState(idint,HAMPTONBAY4_CODE_MED);
              break;
            case FAN_MED:
              fans[idint].fanSpeed=FAN_LOW;
              transmitState(idint,HAMPTONBAY4_CODE_LOW);
              break;
            case FAN_LOW:
              fans[idint].fanSpeed=FAN_LOW;
              transmitState(idint,HAMPTONBAY4_CODE_LOW);
              break;
            default:
              if(fans[idint].fanSpeed<FAN_LOW)
                fans[idint].fanSpeed++;
              break;
          }
        } else if(strcmp(payloadChar,"high") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_HI;
          transmitState(idint,HAMPTONBAY4_CODE_HI);
        } else if(strcmp(payloadChar,"medium") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_MED;
          transmitState(idint,HAMPTONBAY4_CODE_MED);
        } else if(strcmp(payloadChar,"med") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_MED;
          transmitState(idint,HAMPTONBAY4_CODE_MED);
        } else if(strcmp(payloadChar,"low") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_LOW;
          transmitState(idint,HAMPTONBAY4_CODE_LOW);
        } else {
          fans[idint].fanState = false;
          transmitState(idint,HAMPTONBAY4_CODE_OFF);
        }
      } 
    } else {
      // Invalid ID
      return;
    }
  }
  if(strncmp(topic, STAT_BASE_TOPIC, sizeof(STAT_BASE_TOPIC)-1) == 0) {
  
    // Get ID after the base topic + a slash
    char id[5];
    memcpy(id, &topic[sizeof(STAT_BASE_TOPIC)], 4);
    id[4] = '\0';
    if(strspn(id, idchars)) {
      uint8_t idint = strtol(id, (char**) NULL, 2);
      char *attr;
      // Split by slash after ID in topic to get attribute and action
    
      attr = strtok(topic+sizeof(STAT_BASE_TOPIC)-1 + 5, "/");

      if(strcmp(attr,"fan") ==0) {
        if(strcmp(payloadChar,"on") == 0) {
          fans[idint].fanState = true;
        } else {
          fans[idint].fanState = false;
        }
      } else if(strcmp(attr,"speed") ==0) {
        if(strcmp(payloadChar,"high") ==0) {
          fans[idint].fanSpeed = FAN_HI;
        } else if(strcmp(payloadChar,"medium") ==0) {
          fans[idint].fanSpeed = FAN_MED;
        } else if(strcmp(payloadChar,"low") ==0) {
          fans[idint].fanSpeed = FAN_LOW;
        }
      } else if(strcmp(attr,"power") ==0) {
        if(strcmp(payloadChar,"on") == 0) {
          fans[idint].powerState = true;
        } else {
          fans[idint].powerState = false;
        }
      } 
    } else {
      // Invalid ID
      return;
    }
  }
}

void hamptonbay4RF(int long value, int prot, int bits) {
    if( (prot == 15)  && bits == 12 && ((value&0x0c0)==0x0c0)) {
      unsigned long t=millis();
      if(value == lastvalue) {
        if(t - lasttime < NO_RF_REPEAT_TIME)
          return;
        lasttime=t;
      }
      lastvalue=value;
      lasttime=t;
      int dipId = (~(value>>7))&0x0f;
      // Got a correct id in the correct protocol
      if(dipId < 16) {
        // Blank out id in message to get light state
        switch((value)&0x7f) {
          //case 0x7f: // Light
          //  fans[dipId].lightState = !(fans[dipId].lightState);
          //  break;
          case HAMPTONBAY4_CODE_LOW: // Fan Low
            fans[dipId].fanState = true;
            fans[dipId].fanSpeed = FAN_LOW;
            break;
          case HAMPTONBAY4_CODE_MED: // Fan Med
            fans[dipId].fanState = true;
            fans[dipId].fanSpeed = FAN_MED;
            break;
          case HAMPTONBAY4_CODE_HI: // Fan Hi
            fans[dipId].fanState = true;
            fans[dipId].fanSpeed = FAN_HI;
            break;
          case HAMPTONBAY4_CODE_OFF: // Fan Off
            fans[dipId].fanState = false;
            break;
        }
        postStateUpdate(dipId);
      }
    }
}

void hamptonbay4MQTTSub(boolean setup) {
  client.subscribe(SUBSCRIBE_TOPIC_CMND);
  if(setup) client.subscribe(SUBSCRIBE_TOPIC_STAT_SETUP);
}

void hamptonbay4Setup() {
  lasttime=0;
  lastvalue=0;
  // initialize fan struct 
  for(int i=0; i<16; i++) {
    fans[i].powerState = false;
    fans[i].lightState = false;
    fans[i].fanState = false;  
    fans[i].fanSpeed = FAN_LOW;
    fans[i].directionState = false;
  }
}
   
void hamptonbay4SetupEnd() {
  client.unsubscribe(SUBSCRIBE_TOPIC_STAT_SETUP);
}
