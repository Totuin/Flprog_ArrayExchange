#include <flprogUtilites.h>
#include <flprogUart.h>
#include <flprogArrayExchange.h>

// Демонстрационные массивы RAM; в FLProg вместо функций ниже используются
// автоматически сгенерированные readFlprogArray... / writeFlprogArray...
#define UART_NUMBER 1
const uint16_t count = 3;
String names[count] = {"Первый", "Второй", "Третий"};
int16_t values[count] = {10, 20, 30};
bool flags[count] = {true, false, true};
String readName(uint16_t i) { return names[i]; }
void writeName(String v, uint16_t i) { names[i] = v; }
int16_t readValue(uint16_t i) { return values[i]; }
void writeValue(int16_t v, uint16_t i) { values[i] = v; }
bool readFlag(uint16_t i) { return flags[i]; }
void writeFlag(bool v, uint16_t i) { flags[i] = v; }

FlprogUartExecutor FLProgUart;
FlprogExchangeStringArray exchangeNames(1, count, readName, writeName);
FlprogExchangeInt16Array exchangeValues(2, count, readValue, writeValue);
FlprogExchangeBoolArray exchangeFlags(3, count, readFlag, writeFlag);
FlprogAbstractExchangeArray* arrays[] = {&exchangeNames, &exchangeValues, &exchangeFlags};
FlprogUartArrayExchange exchange;

void setup() {
    FLProgUart.setSpeedUart(115200, UART_NUMBER);
    FLProgUart.setDataBitUart(8, UART_NUMBER);
    FLProgUart.setStopBitUart(1, UART_NUMBER);
    FLProgUart.setParityUart(0, UART_NUMBER);
    exchangeNames.setName("Названия");
    exchangeNames.setMaxLength(32);
    exchangeValues.setName("Значения");
    exchangeFlags.setName("Флаги");
    exchange.setUart(&FLProgUart, UART_NUMBER);
    bool ok = exchange.setArrays(arrays, sizeof(arrays) / sizeof(arrays[0]));
    exchange.setWriteEnable(ok); // Только для демонстрации, в автоматике нужен interlock.
}
void loop() {
    exchange.pool();
}
