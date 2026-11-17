#include "flprogArrayExchange.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <iostream>
uint32_t now=0;
uint32_t millis(){return now;}
uint8_t FlprogAbstractUartExecutor::getParityUart(uint8_t){return 0;}
uint8_t FlprogAbstractUartExecutor::getPinRxUart(uint8_t){return 0;}
class Uart:public FlprogAbstractUartExecutor{
public:
    std::string rx,tx;size_t pos=0;
    void beginUart(uint8_t) override{}
    void endUart(uint8_t) override{}
    int availableUart(uint8_t) override{return (int)(rx.size()-pos);}
    uint8_t readUart(uint8_t) override{return rx.at(pos++);}
    void writeUart(uint8_t v,uint8_t) override{tx+=(char)v;}
    void printUart(String v,uint8_t) override{tx+=v.c_str();}
    void writeUart(uint8_t* b,uint16_t n,uint8_t) override{tx.append((char*)b,n);}
    uint32_t getSpeedUart(uint8_t) override{return 115200;}
    void setSpeedUart(uint32_t,uint8_t) override{}
    uint8_t getStatusUart(uint8_t) override{return 1;}
    uint8_t getDataBitUart(uint8_t) override{return 8;}
    void setDataBitUart(uint8_t,uint8_t) override{}
    uint8_t getStopBitUart(uint8_t) override{return 1;}
    void setStopBitUart(uint8_t,uint8_t) override{}
    void setParityUart(uint8_t,uint8_t) override{}
    void setPinRxUart(uint8_t,uint8_t) override{}
    uint8_t getPinTxUart(uint8_t) override{return 0;}
    void setPinTxUart(uint8_t,uint8_t) override{}
    void setPinsUart(uint8_t,uint8_t,uint8_t) override{}
};
int16_t values[]={7,8};bool flags[]={false};String names[]={"old"};int writes=0;bool failWrite=false;
int16_t ri(uint16_t i){return values[i];}void wi(int16_t v,uint16_t i){++writes;if(!failWrite)values[i]=v;}
bool rb(uint16_t i){return flags[i];}void wb(bool v,uint16_t i){++writes;flags[i]=v;}
String rs(uint16_t i){return names[i];}void ws(String v,uint16_t i){++writes;names[i]=v;}
std::string frame(const std::string& s){char b[8];snprintf(b,sizeof b,"*%04X\n",(unsigned)FlprogUartArrayExchange::crc16(s.data(),s.size()));return s+b;}
int main(){
    Uart uart; FlprogUartArrayExchange ex;
    FlprogExchangeInt16Array a(1,2,ri,wi);FlprogExchangeBoolArray b(2,1,rb,wb);FlprogExchangeStringArray c(3,1,rs,ws);
    FlprogExchangeInt16Array ro(4,2,ri,nullptr);FlprogAbstractExchangeArray* arr[]={&a,&b,&c,&ro};
    ex.setUart(&uart,1);assert(ex.setArrays(arr,4));c.setName("Title");c.setMaxLength(20);
    int tests=0;
    auto raw=[&](std::string wire,std::string expected){uart.rx=wire;uart.pos=0;uart.tx.clear();for(int i=0;i<500;++i)ex.pool();assert(uart.tx==frame(expected));++tests;};
    auto send=[&](std::string s,std::string expected,bool crc){raw(crc?frame(s):s+"\n",expected);};
    assert(FlprogUartArrayExchange::crc16("123456789",9)==0x29B1);
    send("PING","OK;PONG",false);send("INFO","OK;INFO;1;4;64;"+std::to_string(FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE)+";64;0",false);
    send("BEGIN;1","ERROR;WRITE_DISABLED",true);ex.setWriteEnable(true);
    send("DESCRIBE;2","OK;ARRAY;3;3;1;3;20;5469746C65",false);send("DESCRIBE;3","OK;ARRAY;4;2;2;1;0;",false);
    send("GET;1;0","OK;VALUE;1;0;7",false);send("GET;1;2","ERROR;INDEX",false);
    send("BEGIN;3","ERROR;CRC_REQUIRED",false);raw("BEGIN;3*0000\n","ERROR;CRC");
    send("BEGIN;3","OK;BEGIN;3",true);send("SET;1;0;-32768","OK;SET;1;0",true);send("SET;1;0;-32768","OK;SET;1;0",true);
    send("SET;1;0;3","ERROR;DUPLICATE",true);send("SET;1;1;32768","ERROR;VALUE",true);send("SET;3;0;C080","ERROR;VALUE",true);
    send("SET;4;0;10","ERROR;WRITE_DISABLED",true);send("SET;2;0;1","OK;SET;2;0",true);send("COMMIT","ERROR;INCOMPLETE",true);assert(writes==0);
    send("SET;3;0;D090","OK;SET;3;0",true);assert(writes==0);send("COMMIT","OK;APPLIED;3",true);assert(writes==3&&values[0]==-32768&&flags[0]&&std::string(names[0].c_str())=="А");
    send("GET;3;0","OK;VALUE;3;0;D090",false);
    send("BEGIN;1","OK;BEGIN;1",true);send("SET;3;0;","OK;SET;3;0",true);send("COMMIT","OK;APPLIED;1",true);assert(names[0].length()==0);
    send("BEGIN;1","OK;BEGIN;1",true);send("SET;1;0;4","OK;SET;1;0",true);send("CANCEL","OK;CANCELLED",true);assert(values[0]==-32768);
    send("BEGIN;1","OK;BEGIN;1",true);send("SET;1;0;4","OK;SET;1;0",true);now+=10000;send("COMMIT","ERROR;NO_TRANSACTION",true);assert(values[0]==-32768);
    send("BEGIN;1","OK;BEGIN;1",true);send("SET;1;0;4","OK;SET;1;0",true);ex.setWriteEnable(false);ex.setWriteEnable(true);send("COMMIT","ERROR;NO_TRANSACTION",true);
    assert(a.setLimits(0,100));send("BEGIN;1","OK;BEGIN;1",true);send("SET;1;0;101","ERROR;VALUE",true);send("SET;1;0;100","OK;SET;1;0",true);a.setLimits(0,99);send("COMMIT","ERROR;PRECHECK",true);assert(values[0]==-32768);
    send("BEGIN;2","OK;BEGIN;2",true);send("SET;2;0;0","OK;SET;2;0",true);send("SET;1;0;50","OK;SET;1;0",true);failWrite=true;send("COMMIT","ERROR;APPLY_PARTIAL;1;1;0",true);assert(!flags[0]);failWrite=false;
    raw(std::string(220,'X')+"\n","ERROR;LINE");raw("PING\r\n","OK;PONG");
    // Verify buffer exhaustion with unique staged strings; no callbacks run before COMMIT.
    FlprogExchangeStringArray many(9,64,rs,ws);
    FlprogAbstractExchangeArray* large[]={&many};assert(ex.setArrays(large,1));
    unsigned full=FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE/135;
    if(full < 64 && (full+1)*7 <= FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE){
        int savedWrites=writes;std::string count=std::to_string(full+1);
        send("BEGIN;"+count,"OK;BEGIN;"+count,true);
        for(unsigned i=0;i<=full;++i){std::string ix=std::to_string(i);
            send("SET;9;"+ix+";"+std::string(128,'4'), i==full?"ERROR;BUFFER":"OK;SET;9;"+ix,true);
        }
        send("CANCEL","OK;CANCELLED",true);assert(writes==savedWrites);
    }
    FlprogAbstractExchangeArray* duplicate[]={&a,&a};assert(!ex.setArrays(duplicate,2));
    std::cout<<"PASS: "<<tests<<" protocol exchanges, CRC vector, assertions on data, registry, and write deferral\n";
}
