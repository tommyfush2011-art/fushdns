#include <Arduino.h>
const char* domains[] = {"doubleclick.net","googlesyndication.com","googleadservices.com","adnxs.com","adsrvr.org","rubiconproject.com","openx.net","pubmatic.com"};
char cmd[32]; size_t pos=0;
void setup(){ Serial.begin(115200); }
void loop(){ while(Serial.available()){ char c=Serial.read(); if(c=='\n'){cmd[pos]=0; if(!strcmp(cmd,"LIST")){Serial.println("FUSH-BEGIN"); for(auto d:domains) Serial.println(d); Serial.println("FUSH-END");} pos=0;} else if(c!='\r' && pos<sizeof(cmd)-1)cmd[pos++]=c; } delay(1); }
