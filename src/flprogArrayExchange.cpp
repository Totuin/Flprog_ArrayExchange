#include "flprogArrayExchange.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
const char hexDigits[] = "0123456789ABCDEF";
int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
bool parseUInt(const char* s, uint16_t& out) {
    if (!s || !*s) return false;
    uint32_t n = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return false;
        n = n * 10 + (*s - '0');
        if (n > 65535UL) return false;
    }
    out = (uint16_t)n;
    return true;
}
bool parseInt16(const char* s, int16_t& out) {
    if (!s || !*s) return false;
    bool neg = *s == '-';
    if (neg) ++s;
    uint16_t n;
    if (!parseUInt(s, n) || n > (neg ? 32768UL : 32767UL)) return false;
    out = (int16_t)(neg ? -(int32_t)n : (int32_t)n);
    return true;
}
bool encodeHex(const char* s, size_t n, char* out, size_t cap) {
    if (!s || !out || cap == 0 || n > (cap - 1) / 2) return false;
    for (size_t i=0; i<n; ++i) {
        uint8_t b = (uint8_t)s[i];
        if (!b) return false;
        out[2*i] = hexDigits[b >> 4]; out[2*i+1] = hexDigits[b & 15];
    }
    out[2*n] = 0; return true;
}
// Проверка корректного UTF-8, без NUL, включая запрет суррогатов и overlong.
bool validUtf8(const uint8_t* p, size_t n) {
    for (size_t i=0; i<n;) {
        uint8_t c=p[i++]; if (!c) return false; if (c<0x80) continue;
        uint8_t k; uint32_t cp; uint32_t min;
        if (c>=0xC2 && c<=0xDF) { k=1; cp=c&31; min=0x80; }
        else if (c>=0xE0 && c<=0xEF) { k=2; cp=c&15; min=0x800; }
        else if (c>=0xF0 && c<=0xF4) { k=3; cp=c&7; min=0x10000; }
        else return false;
        if (n-i<k) return false;
        while (k--) { uint8_t t=p[i++]; if ((t&0xC0)!=0x80) return false; cp=(cp<<6)|(t&63); }
        if (cp<min || cp>0x10FFFFUL || (cp>=0xD800 && cp<=0xDFFF)) return false;
    }
    return true;
}
bool decodeHex(const char* s, uint8_t* out, size_t limit, size_t& n) {
    if (!s) return false;
    size_t len=strlen(s); n=len/2;
    if ((len&1) || n>limit) return false;
    for (size_t i=0; i<n; ++i) {
        int h=hexNibble(s[2*i]), l=hexNibble(s[2*i+1]);
        if (h<0 || l<0) return false;
        out[i]=(uint8_t)((h<<4)|l);
    }
    return validUtf8(out,n);
}
void put16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
uint16_t get16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
}

bool FlprogExchangeBoolArray::encode(uint16_t i,char* out,size_t cap) const {
    if (!canRead() || i>=getSize() || cap<2) return false;
    out[0]=_read(i)?'1':'0'; out[1]=0; return true;
}
bool FlprogExchangeBoolArray::validate(const char* s) const { return s && (s[0]=='0'||s[0]=='1') && !s[1]; }
bool FlprogExchangeBoolArray::apply(uint16_t i,const char* s) {
    if (!canWrite() || i>=getSize() || !validate(s)) return false;
    bool v=s[0]=='1'; _write(v,i); return !canRead() || _read(i)==v;
}
bool FlprogExchangeInt16Array::encode(uint16_t i,char* out,size_t cap) const {
    if (!canRead() || i>=getSize()) return false;
    int n=snprintf(out,cap,"%ld",(long)_read(i)); return n>=0 && (size_t)n<cap;
}
bool FlprogExchangeInt16Array::validate(const char* s) const { int16_t v; return parseInt16(s,v) && v >= _min && v <= _max; }
bool FlprogExchangeInt16Array::apply(uint16_t i,const char* s) {
    int16_t v;
    if (!canWrite() || i>=getSize() || !parseInt16(s,v) || v < _min || v > _max) return false;
    _write(v,i); return !canRead() || _read(i)==v;
}
bool FlprogExchangeStringArray::encode(uint16_t i,char* out,size_t cap) const {
    if (!canRead() || i>=getSize()) return false;
    String v=_read(i); size_t n=v.length();
    return n<=_maxLength && validUtf8((const uint8_t*)v.c_str(),n) && encodeHex(v.c_str(),n,out,cap);
}
bool FlprogExchangeStringArray::validate(const char* s) const {
    uint8_t bytes[FLPROG_EXCHANGE_MAX_STRING_BYTES]; size_t n;
    return decodeHex(s,bytes,_maxLength,n);
}
bool FlprogExchangeStringArray::apply(uint16_t i,const char* s) {
    uint8_t bytes[FLPROG_EXCHANGE_MAX_STRING_BYTES]; size_t n;
    if (!canWrite() || i>=getSize() || !decodeHex(s,bytes,_maxLength,n)) return false;
    String v;
    if (n && !v.reserve(n)) return false;
    for (size_t j=0;j<n;++j) if (!v.concat((char)bytes[j])) return false;
    _write(v,i);
    if (!canRead()) return true;
    String back=_read(i);
    return back.length()==n && (!n || memcmp(back.c_str(),bytes,n)==0);
}

void FlprogUartArrayExchange::resetTransaction() { _active=false; _used=0; _received=0; _expected=0; }
void FlprogUartArrayExchange::setUart(FlprogAbstractUartExecutor* uart,uint8_t number) {
    resetTransaction(); _uart=uart; _port=number; _rxLen=_txLen=_txPos=0; _discard=false;
}
bool FlprogUartArrayExchange::setArrays(FlprogAbstractExchangeArray* const* arrays,uint16_t count) {
    resetTransaction(); _arrays=nullptr; _count=0;
    if (!arrays && count) return false;
    for (uint16_t i=0;i<count;++i) {
        if (!arrays[i]) return false;
        for (uint16_t j=0;j<i;++j) if (arrays[i]->getId()==arrays[j]->getId()) return false;
    }
    _arrays=arrays; _count=count; return true;
}
void FlprogUartArrayExchange::setWriteEnable(bool enable) { _writeEnable=enable; if (!enable) resetTransaction(); }
FlprogAbstractExchangeArray* FlprogUartArrayExchange::find(uint16_t id) const {
    for (uint16_t i=0;i<_count;++i) if (_arrays[i]->getId()==id) return _arrays[i];
    return nullptr;
}
uint16_t FlprogUartArrayExchange::crc16(const char* data,size_t len) {
    uint16_t crc=0xFFFF;
    for (size_t i=0;i<len;++i) { crc^=(uint16_t)(uint8_t)data[i]<<8;
        for (uint8_t b=0;b<8;++b) crc=(uint16_t)((crc&0x8000)?(crc<<1)^0x1021:crc<<1); }
    return crc;
}
void FlprogUartArrayExchange::reply(const char* fmt,...) {
    va_list args; va_start(args,fmt); int n=vsnprintf(_tx,sizeof(_tx)-6,fmt,args); va_end(args);
    if (n<0 || (size_t)n>=sizeof(_tx)-6) { strcpy(_tx,"ERROR;RESPONSE_TOO_LONG"); n=(int)strlen(_tx); }
    uint16_t crc=crc16(_tx,(size_t)n);
    snprintf(_tx+n,sizeof(_tx)-(size_t)n,"*%04X\n",(unsigned int)crc);
    _txLen=(uint16_t)(n+6); _txPos=0;
}
void FlprogUartArrayExchange::error(const char* code) { reply("ERROR;%s",code); }
void FlprogUartArrayExchange::flushTx() {
    uint16_t budget=_txBudget;
    while (budget-- && _txPos<_txLen) _uart->writeUart((uint8_t)_tx[_txPos++],_port);
    if (_txPos==_txLen) _txLen=_txPos=0;
}

void FlprogUartArrayExchange::processLine() {
    bool checked=false;
    char* star=strrchr(_rx,'*');
    if (star) {
        if (strlen(star+1)!=4) { error("CRC"); return; }
        uint16_t sent=0;
        for (uint8_t i=1;i<=4;++i) { int h=hexNibble(star[i]); if(h<0){error("CRC");return;} sent=(uint16_t)((sent<<4)|h); }
        if (crc16(_rx,(size_t)(star-_rx))!=sent) { error("CRC"); return; }
        *star=0; checked=true;
    }
    // Разделение с сохранением пустого последнего поля (пустая строка).
    char* parts[4]={_rx,nullptr,nullptr,nullptr}; uint8_t count=1;
    for (char* p=_rx; *p; ++p) if (*p==';') {
        if (count==4) { error("FIELDS"); return; } *p=0; parts[count++]=p+1;
    }
    const char* cmd=parts[0];
    if (!strcmp(cmd,"PING") && count==1) { reply("OK;PONG"); return; }
    if (!strcmp(cmd,"INFO") && count==1) {
        reply("OK;INFO;%u;%u;%u;%u;%u;%u",(unsigned)FLPROG_EXCHANGE_PROTOCOL_VERSION,(unsigned)_count,
            (unsigned)FLPROG_EXCHANGE_MAX_STRING_BYTES,(unsigned)sizeof(_stage),(unsigned)FLPROG_EXCHANGE_MAX_TRANSACTION_ITEMS,(unsigned)_writeEnable); return;
    }
    if (!strcmp(cmd,"DESCRIBE") && count==2) {
        uint16_t slot; if (!parseUInt(parts[1],slot)||slot>=_count) {error("SLOT");return;}
        auto* a=_arrays[slot]; char name[2*FLPROG_EXCHANGE_MAX_NAME_BYTES+1]; size_t n=strlen(a->getName());
        if(n>FLPROG_EXCHANGE_MAX_NAME_BYTES || !validUtf8((const uint8_t*)a->getName(),n) || !encodeHex(a->getName(),n,name,sizeof(name))) {error("NAME");return;}
        unsigned flags=(a->canRead()?FLPROG_EXCHANGE_READ:0)|(a->canWrite()&&_writeEnable?FLPROG_EXCHANGE_WRITE:0);
        reply("OK;ARRAY;%u;%u;%u;%u;%u;%s",(unsigned)a->getId(),(unsigned)a->getType(),(unsigned)a->getSize(),flags,(unsigned)a->getMaxLength(),name);return;
    }
    if (!strcmp(cmd,"GET") && count==3) {
        uint16_t id,index; if(!parseUInt(parts[1],id)||!parseUInt(parts[2],index)){error("NUMBER");return;}
        auto* a=find(id); if(!a){error("ID");return;} if(index>=a->getSize()){error("INDEX");return;}
        if(!a->canRead()){error("READ_DISABLED");return;}
        char value[2*FLPROG_EXCHANGE_MAX_STRING_BYTES+1]; if(!a->encode(index,value,sizeof(value))){error("READ_VALUE");return;}
        reply("OK;VALUE;%u;%u;%s",(unsigned)id,(unsigned)index,value);return;
    }
    bool begin=!strcmp(cmd,"BEGIN"), set=!strcmp(cmd,"SET"), commit=!strcmp(cmd,"COMMIT"), cancel=!strcmp(cmd,"CANCEL");
    if(!begin&&!set&&!commit&&!cancel){error("COMMAND");return;}
    if(!checked){error("CRC_REQUIRED");return;}
    if((begin&&count!=2)||(set&&count!=4)||((commit||cancel)&&count!=1)){error("FIELDS");return;}
    if(cancel){resetTransaction();reply("OK;CANCELLED");return;}
    if(!_writeEnable){resetTransaction();error("WRITE_DISABLED");return;}
    if(begin){
        if(_active){error("ACTIVE");return;} uint16_t items;
        if(!parseUInt(parts[1],items)||!items||items>FLPROG_EXCHANGE_MAX_TRANSACTION_ITEMS|| (uint32_t)items*7>sizeof(_stage)){error("COUNT");return;}
        _active=true;_expected=items;_used=_received=0;_lastActivity=millis();reply("OK;BEGIN;%u",(unsigned)items);return;
    }
    if(!_active){error("NO_TRANSACTION");return;}
    if(set){
        uint16_t id,index; if(!parseUInt(parts[1],id)||!parseUInt(parts[2],index)){error("NUMBER");return;}
        auto* a=find(id); if(!a){error("ID");return;} if(index>=a->getSize()){error("INDEX");return;}
        if(!a->canWrite()){error("WRITE_DISABLED");return;} if(!a->validate(parts[3])){error("VALUE");return;}
        uint16_t len=(uint16_t)(strlen(parts[3])+1);
        for(uint16_t off=0;off<_used;){const uint8_t* r=_stage+off; uint16_t oldLen=get16(r+4);
            if(get16(r)==id&&get16(r+2)==index){
                if(oldLen==len&&!memcmp(r+6,parts[3],len)){_lastActivity=millis();reply("OK;SET;%u;%u",(unsigned)id,(unsigned)index);}
                else { error("DUPLICATE"); }
                return;
            } off=(uint16_t)(off+6+oldLen);
        }
        if(_received>=_expected){error("COUNT");return;}
        if((uint32_t)_used+6+len>sizeof(_stage)){error("BUFFER");return;}
        uint8_t* r=_stage+_used; put16(r,id);put16(r+2,index);put16(r+4,len);memcpy(r+6,parts[3],len);
        _used=(uint16_t)(_used+6+len);++_received;_lastActivity=millis();reply("OK;SET;%u;%u",(unsigned)id,(unsigned)index);return;
    }
    // COMMIT: предварительная проверка всех записей, затем применение в одном вызове pool().
    if(_received!=_expected){error("INCOMPLETE");return;}
    for(uint16_t off=0;off<_used;){const uint8_t* r=_stage+off; auto* a=find(get16(r));
        if(!a||!a->canWrite()||get16(r+2)>=a->getSize()||!a->validate((const char*)r+6)){resetTransaction();error("PRECHECK");return;}
        off=(uint16_t)(off+6+get16(r+4));
    }
    uint16_t applied=0;
    for(uint16_t off=0;off<_used;){const uint8_t* r=_stage+off; auto* a=find(get16(r));
        if(!a->apply(get16(r+2),(const char*)r+6)){uint16_t id=get16(r),idx=get16(r+2);resetTransaction();reply("ERROR;APPLY_PARTIAL;%u;%u;%u",(unsigned)applied,(unsigned)id,(unsigned)idx);return;}
        ++applied;off=(uint16_t)(off+6+get16(r+4));
    }
    resetTransaction();reply("OK;APPLIED;%u",(unsigned)applied);
}

void FlprogUartArrayExchange::pool() {
    if(!_uart||_port==255)return;
    if(_active && (uint32_t)(millis()-_lastActivity)>=_timeout) resetTransaction();
    if(_txLen){flushTx();return;}
    // availableUart() сам инициализирует аппаратный порт при первом опросе.
    uint16_t budget=_rxBudget;
    while(budget-- && _uart->availableUart(_port)>0){
        uint8_t c=_uart->readUart(_port);
        if(c=='\n'){
            if(_discard){_discard=false;_rxLen=0;error("LINE");return;}
            if(_rxLen && _rx[_rxLen-1]=='\r')--_rxLen;
            if(!_rxLen)continue;
            _rx[_rxLen]=0;processLine();_rxLen=0;return;
        }
        if(_discard)continue;
        if((c<32 && c!='\r') || c>126 || _rxLen>=sizeof(_rx)-1){_discard=true;_rxLen=0;continue;}
        _rx[_rxLen++]=(char)c;
    }
}
