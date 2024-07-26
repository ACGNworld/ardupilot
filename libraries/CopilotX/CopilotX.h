#pragma once

#include <AP_HAL/AP_HAL.h>
#include <AP_HAL/utility/Socket.h>
#include <AP_Common/AP_Common.h>

class CopilotX
{
public:
    bool init();
    void update();
    float eIMUdata[11];

private:
    AP_HAL::UARTDriver *_uart;
};

void PosResolve(char name, char *body, int length);

class ComStr
{
private:
    __uint8_t *buff = NULL;
    int buffSize = 0;
    int count = 0;
    short mark = 0; // 0为无数字，1为记录cx，2为中间，3为cy

    void (*resolveCallback)(char name, char *body, int length) = NULL;
    void (*sendCallback)(char *buff, int length) = NULL;
    bool (*forwardCallback)(char name, char *buff, int length) = NULL;

    void resolveCommand(int length);

public:
    ComStr();
    ~ComStr();

    // 设置缓冲区大小
    bool setBufferSize(unsigned int size);
    // 设置解析回调
    void setResolveCallback(void (*resolveCallback)(char name, char *body, int length));
    // 设置发送数据回调
    void setSendCallback(void (*sendCallback)(char *buff, int length));
    // 设置转发回调
    void setForwardCallback(bool (*forwardCallback)(char name, char *buff, int length));

    // 发送指令
    void sendCommand(char name, const char *body, bool isVerify = true);

    // 往里添加数据进行解析
    void addData(__uint8_t data);

    char *getQueryParam(char *des, const char *urlQuery, const char *paramsName);
    int getQueryParamToInt(const char *urlQuery, const char *paramsName);
    void strcatInt(char *des, int num);
    void resError(char name, int id, const char *errMsg);
    void resSuccess(char name, int id, const char *data);
};

void int2str(int n, char *str);