
#define HAMPTONBAY4_CODE_HI 0x5F
#define HAMPTONBAY4_CODE_MED 0x6F
#define HAMPTONBAY4_CODE_LOW 0x77
#define HAMPTONBAY4_CODE_OFF 0x7D

void hamptonbay4MQTT(char* topic, char* payloadChar, unsigned int length);
void hamptonbay4RF(int long value, int prot, int bits);
void hamptonbay4MQTTSub(boolean setup);
void hamptonbay4Setup();
void hamptonbay4SetupEnd();
            
