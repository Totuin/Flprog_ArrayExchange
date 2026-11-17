#pragma once
#include <Arduino.h>
#include <flprogUartBase.h>
#include <stddef.h>
#include <stdint.h>

#define FLPROG_EXCHANGE_TYPE_BOOL 1
#define FLPROG_EXCHANGE_TYPE_INT16 2
#define FLPROG_EXCHANGE_TYPE_STRING 3
#define FLPROG_EXCHANGE_READ 1
#define FLPROG_EXCHANGE_WRITE 2
#define FLPROG_EXCHANGE_MAX_STRING_BYTES 64
#define FLPROG_EXCHANGE_MAX_NAME_BYTES 32
#define FLPROG_EXCHANGE_MAX_TRANSACTION_ITEMS 64
#define FLPROG_EXCHANGE_LINE_SIZE 192
#define FLPROG_EXCHANGE_PROTOCOL_VERSION 1

// Настраивается единым флагом сборки для всех единиц трансляции.
#ifndef FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE
#define FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE 1024
#endif
static_assert(FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE >= 7 &&
              FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE <= 65535UL,
              "Invalid exchange transaction buffer size");

// Адаптер не владеет массивом. Имя должно жить не меньше адаптера.
class FlprogAbstractExchangeArray
{
public:
    FlprogAbstractExchangeArray(uint16_t id, uint16_t size) : _id(id), _size(size) {}
    virtual ~FlprogAbstractExchangeArray() {}
    uint16_t getId() const { return _id; }
    uint16_t getSize() const { return _size; }
    const char* getName() const { return _name; }
    void setName(const char* name) { _name = name ? name : ""; }
    void setWriteEnable(bool enable) { _writeEnable = enable; }
    virtual uint8_t getType() const = 0;
    virtual uint16_t getMaxLength() const { return 0; }
    virtual bool canRead() const = 0;
    virtual bool canWrite() const = 0;
    // Внутренний интерфейс протокола: текстовое представление одного элемента.
    virtual bool encode(uint16_t index, char* out, size_t capacity) const = 0;
    virtual bool validate(const char* text) const = 0;
    // false означает ошибку применения; запись уже могла произойти.
    virtual bool apply(uint16_t index, const char* text) = 0;
protected:
    bool writeEnabled() const { return _writeEnable; }
private:
    uint16_t _id, _size;
    const char* _name = "";
    bool _writeEnable = true;
};

template<class T> class FlprogExchangeTypedArray : public FlprogAbstractExchangeArray
{
public:
    using ReadFunction = T (*)(uint16_t index);
    using WriteFunction = void (*)(T value, uint16_t index);
    FlprogExchangeTypedArray(uint16_t id, uint16_t size, ReadFunction read = nullptr, WriteFunction write = nullptr)
        : FlprogAbstractExchangeArray(id, size), _read(read), _write(write) {}
    // Назначать функции в setup(), до начала обмена. nullptr отключает направление.
    void setReadFunction(ReadFunction read) { _read = read; }
    void setWriteFunction(WriteFunction write) { _write = write; }
    bool canRead() const override { return _read != nullptr; }
    bool canWrite() const override { return _write != nullptr && writeEnabled(); }
protected:
    ReadFunction _read;
    WriteFunction _write;
};

class FlprogExchangeBoolArray : public FlprogExchangeTypedArray<bool>
{
public:
    using FlprogExchangeTypedArray<bool>::FlprogExchangeTypedArray;
    uint8_t getType() const override { return FLPROG_EXCHANGE_TYPE_BOOL; }
    bool encode(uint16_t index, char* out, size_t capacity) const override;
    bool validate(const char* text) const override;
    bool apply(uint16_t index, const char* text) override;
};
class FlprogExchangeInt16Array : public FlprogExchangeTypedArray<int16_t>
{
public:
    using FlprogExchangeTypedArray<int16_t>::FlprogExchangeTypedArray;
    uint8_t getType() const override { return FLPROG_EXCHANGE_TYPE_INT16; }
    bool setLimits(int16_t minimum, int16_t maximum) {
        if (minimum > maximum) return false;
        _min = minimum; _max = maximum; return true;
    }
    bool encode(uint16_t index, char* out, size_t capacity) const override;
    bool validate(const char* text) const override;
    bool apply(uint16_t index, const char* text) override;
private:
    int16_t _min = INT16_MIN, _max = INT16_MAX;
};
class FlprogExchangeStringArray : public FlprogExchangeTypedArray<String>
{
public:
    using FlprogExchangeTypedArray<String>::FlprogExchangeTypedArray;
    uint8_t getType() const override { return FLPROG_EXCHANGE_TYPE_STRING; }
    // Лимит в байтах UTF-8, не в символах. 0 допускает только пустую строку.
    void setMaxLength(uint16_t bytes) { _maxLength = bytes > FLPROG_EXCHANGE_MAX_STRING_BYTES ? FLPROG_EXCHANGE_MAX_STRING_BYTES : bytes; }
    uint16_t getMaxLength() const override { return _maxLength; }
    bool encode(uint16_t index, char* out, size_t capacity) const override;
    bool validate(const char* text) const override;
    bool apply(uint16_t index, const char* text) override;
private:
    uint16_t _maxLength = FLPROG_EXCHANGE_MAX_STRING_BYTES;
};

class FlprogUartArrayExchange
{
public:
    void setUart(FlprogAbstractUartExecutor* uart, uint8_t number);
    // Массив указателей и адаптеры должны существовать всё время обмена.
    // false: неверный реестр, нулевой указатель или повторяющийся ID.
    bool setArrays(FlprogAbstractExchangeArray* const* arrays, uint16_t count);
    // По умолчанию запись запрещена. false отменяет незавершённую транзакцию.
    void setWriteEnable(bool enable);
    bool isTransactionActive() const { return _active; }
    void setTimeout(uint32_t ms) { _timeout = ms ? ms : 1; }
    void setRxBudget(uint16_t bytes) { _rxBudget = bytes ? bytes : 1; }
    void setTxBudget(uint16_t bytes) { _txBudget = bytes ? bytes : 1; }
    void pool();
    static uint16_t crc16(const char* data, size_t length);
private:
    FlprogAbstractUartExecutor* _uart = nullptr;
    uint8_t _port = 255;
    FlprogAbstractExchangeArray* const* _arrays = nullptr;
    uint16_t _count = 0;
    // Собственный буфер: без внешнего массива и динамического выделения.
    uint8_t _stage[FLPROG_EXCHANGE_TRANSACTION_BUFFER_SIZE];
    uint16_t _used = 0, _expected = 0, _received = 0;
    bool _writeEnable = false, _active = false, _discard = false;
    uint32_t _lastActivity = 0, _timeout = 10000;
    uint16_t _rxBudget = 32, _txBudget = 16;
    char _rx[FLPROG_EXCHANGE_LINE_SIZE] = {};
    char _tx[FLPROG_EXCHANGE_LINE_SIZE] = {};
    uint16_t _rxLen = 0, _txLen = 0, _txPos = 0;
    void resetTransaction();
    FlprogAbstractExchangeArray* find(uint16_t id) const;
    void processLine();
    void reply(const char* format, ...);
    void error(const char* code);
    void flushTx();
};
