#include "CopilotX.h"
#include <stdio.h>
#include <stdlib.h>

ComStr command;
void UartInit();
void setup();
void loop();

// extern const AP_HAL::HAL &hal;
const AP_HAL::HAL &hal = AP_HAL::get_HAL();

bool CopilotX::init() {
    _uart = hal.serial(1); // 使用uartA作为串口
    hal.scheduler->delay(200);
    if (_uart == nullptr) {
        return false;
    }
    _uart->begin(115200); // 初始化串口，波特率为115200
    UartInit();
    return true;
}

void CopilotX::update() {
    int i = _uart->available();
    while (i--)
    {
        command.addData(_uart->read());
    }
}

void UartInit()
{

    // 设置缓冲区大小
    command.setBufferSize(2048);
    // 设置解析指令回调，调用command.addData就会执行ResolveCommand解析出指令,然后执行（）里的函数
    command.setResolveCallback(PosResolve);

    // 设置发送指令回调，调用command.sendCommand就会制作整帧，然后执行（）里的函数发出
    command.setSendCallback([](char *buff, int length) {});
    // command.setSendCallback(mySendCallback);  //自己改就这么改 void mySendCallback(char *buff, int length);
}

void PosResolve(char name, char *body, int length)
{

    hal.serial(0)->printf("name:%c,  body:%s\n", name, body);
}

//********************类函数定义

ComStr::ComStr()
{
}

ComStr::~ComStr()
{
    if (buff != NULL)
    {
        free(buff);
        buff = NULL;
    }
}

bool ComStr::setBufferSize(unsigned int size)
{
    buff = (__uint8_t *)malloc(sizeof(__uint8_t) * size);
    if (buff == NULL)
    {
        return false;
    }
    buffSize = size;
    memset(buff, 0, size);
    return true;
}

void ComStr::setResolveCallback(void (*newResolveCallback)(char name, char *body, int length))
{
    this->resolveCallback = newResolveCallback;
}

void ComStr::setSendCallback(void (*newSendCallback)(char *buff, int length))
{
    this->sendCallback = newSendCallback;
}

void ComStr::setForwardCallback(bool (*newForwardCallback)(char name, char *buff, int length))
{
    this->forwardCallback = newForwardCallback;
}

void ComStr::resolveCommand(int length)
{
    if (length < 5)
    {
        return;
    }
    if (!(*(buff + length - 3) == '|' || *(buff + length - 6) == '|'))
    {
        return;
    }

    if (resolveCallback == NULL)
    {
        return;
    }

    char name = 0;    // 最后一个是名称, 就一个字符
    char verify1 = 0; // 倒数第二个是验证码, 两个字符 可有可无
    char verify2 = 0; // 倒数第二个是验证码, 两个字符 可有可无
    int dataEndIndex = length;
    if (*(buff + length - 6) == '|')
    {
        // 有验证码 和 指令项
        name = *(buff + length - 5);
        verify1 = *(buff + length - 3);
        verify2 = *(buff + length - 2);
        dataEndIndex = dataEndIndex - 4;
    }
    else if (*(buff + length - 3) == '|')
    {
        // 只有指令项, 无验证码
        verify1 = 0;
        verify2 = 0;
        name = *(buff + length - 2);
        dataEndIndex = dataEndIndex - 1;
    }
    if (name == 0)
    {
        return;
    }
    if (verify1 > 0)
    {
        uint32_t sum = 0;
        for (int i = 1; i < dataEndIndex; i++)
        {
            sum += *(buff + i);
        }
        char bodyVerify1 = (sum & 0x00ff) % ('z' - ' ') + ' ';
        char bodyVerify2 = ((sum & 0x00ff00) >> 8) % ('z' - ' ') + ' ';

        if (!(bodyVerify1 == verify1 && bodyVerify2 == verify2))
        {
            return;
        }
    }
    if (forwardCallback != NULL)
    {
        // 用于数据转发
        bool isContinue = forwardCallback(name, (char *)buff, length);
        if (!isContinue)
        {
            return;
        }
    }

    char body[dataEndIndex - 2] = {0}; // 字符串数据部分
    memset(body, 0, dataEndIndex - 2);
    dataEndIndex = dataEndIndex - 2; // 去掉 |name
    for (int i = 1; i < dataEndIndex; i++)
    {
        body[i - 1] = *(buff + i);
    }

    resolveCallback(name, body, dataEndIndex - 1);
}

// 格式：{X:%d,Y:%d,W:%d,H:%d|%d}
void ComStr::addData(__uint8_t data)
{
    // 重新开始
    if (data == '{')
    {
        *buff = data;
        count = 1;
        return;
    }
    if (count < 1)
    {
        count = 0;
        return;
    }

    if (count >= buffSize)
    {
        count = 0;
        return;
    }
    // 过滤掉其他非字符
    if (data < ' ' || data > '}')
    {
        count = 0;
        return;
    }

    *(buff + count) = data;
    count++;

    // 结束字符
    if (data == '}')
    {
        resolveCommand(count);
        count = 0;
    }
}

// 发送指令
void ComStr::sendCommand(char name, const char *body, bool isVerify)
{
    int length = strlen(body);
    char cmd_to_send[length + 8] = {0};
    memset(cmd_to_send, 0, length + 8);
    cmd_to_send[0] = '{';

    for (int i = 0; i < length; i++)
    {
        cmd_to_send[i + 1] = *(body + i);
    }

    int strLength = 0;
    if (isVerify)
    {
        cmd_to_send[length + 1] = '|';
        cmd_to_send[length + 2] = name;
        // 计算校验和
        uint32_t sum = 0;
        int verifyLen = length + 3;
        for (int i = 1; i < verifyLen; i++)
        {
            sum += *(cmd_to_send + i);
        }

        cmd_to_send[length + 3] = '|';
        cmd_to_send[length + 4] = (sum & 0x00ff) % ('z' - ' ') + ' ';
        cmd_to_send[length + 5] = ((sum & 0x00ff00) >> 8) % ('z' - ' ') + ' ';
        cmd_to_send[length + 6] = '}';
        strLength = length + 7;
    }
    else
    {
        cmd_to_send[length + 1] = '|';
        cmd_to_send[length + 2] = name;
        cmd_to_send[length + 3] = '}';
        strLength = length + 4;
    }

    if (sendCallback != NULL)
    {
        sendCallback(cmd_to_send, strLength);
    }
}

/**
 * 从数据体中获取参数值
 */
char *ComStr::getQueryParam(char *des, const char *urlQuery, const char *paramsName)
{
    int urlQueryLen = strlen(urlQuery);
    int paramsNameLen = strlen(paramsName);

    for (int i = 0; i < urlQueryLen; i++)
    {
        if (i >= urlQueryLen - paramsNameLen)
        {
            break;
        }
        int start = i;
        bool isSame = true;
        for (int j = 0; j < paramsNameLen; j++)
        {
            if (urlQuery[start] != paramsName[j])
            {
                isSame = false;
                break;
            }
            start++;
        }
        if (isSame)
        {
            i = start;
            if (urlQuery[i] == '=')
            {
                i++;
                while (urlQuery[i])
                {
                    if (urlQuery[i] == '&')
                    {
                        return des;
                    }
                    *des++ = urlQuery[i];
                    i++;
                    if (i > 1000)
                    {
                        break;
                    }
                }
                return des;
            }
        }
    }
    return des;
}

/**
 * 从数据体中获取参数值, 并转化成int类型返回
 *
 */
int ComStr::getQueryParamToInt(const char *urlQuery, const char *paramsName)
{
    char dataChar[11] = {0};
    ComStr::getQueryParam(dataChar, urlQuery, paramsName);
    if (strlen(dataChar))
    {
        return atol(dataChar);
    }
    return INT32_MAX;
}

void ComStr::strcatInt(char *des, int num)
{
    char numStr[11] = {0};
    int2str(num, numStr);
    strcat(des, numStr);
}

void ComStr::resSuccess(char name, int id, const char *data)
{
    char body[128] = {0};
    memset(body, 0, 128);
    strcat(body, "id=");
    strcatInt(body, id);
    strcat(body, "&");
    strcat(body, data);
    sendCommand(name, body);
}

void ComStr::resError(char name, int id, const char *errMsg)
{
    char body[128] = {0};
    memset(body, 0, 128);
    strcat(body, "id=");
    strcatInt(body, id);
    strcat(body, "&msg=");
    strcat(body, errMsg);
    sendCommand(name, body);
}

void int2str(int n, char *str)
{
    char buf[10] = "";
    int i = 0;
    int len = 0;
    int temp = n < 0 ? -n : n; // temp为n的绝对值

    if (str == NULL)
    {
        return;
    }
    while (temp)
    {
        buf[i++] = (temp % 10) + '0'; // 把temp的每一位上的数存入buf
        temp = temp / 10;
    }

    len = n < 0 ? ++i : i; // 如果n是负数，则多需要一位来存储负号
    str[i] = 0;            // 末尾是结束符0
    while (1)
    {
        i--;
        if (buf[len - i - 1] == 0)
        {
            break;
        }
        str[i] = buf[len - i - 1]; // 把buf数组里的字符拷到字符串
    }
    if (i == 0)
    {
        str[i] = '-'; // 如果是负数，添加一个负号
    }
}