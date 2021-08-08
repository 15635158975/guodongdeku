/* main.c -- MQTT client example
 *
 * Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of Redis nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//															//																		//
// 工程：	MQTT_JX											//	注：在《esp_mqtt_proj》例程上修改									//
//															//																		//
// 平台：	【技新电子】物联网开发板 ESP8266 V1.0			//	①：添加【详实的注释】唉，不说了，说多了都是泪！！！				//
//															//																		//
// 功能：	①：设置MQTT相关参数							//	②：修改【MQTT参数数组】config.h -> device_id/mqtt_host/mqtt_pass	//
//															//																		//
//			②：与MQTT服务端，建立网络连接(TCP)				//	③：修改【MQTT_CLIENT_ID宏定义】mqtt_config.h -> MQTT_CLIENT_ID		//
//															//																		//
//			③：配置/发送【CONNECT】报文，连接MQTT服务端	//	④：修改【PROTOCOL_NAMEv31宏】mqtt_config.h -> PROTOCOL_NAMEv311	//
//															//																		//
//			④：订阅主题"SW_LED"							//	⑤：修改【心跳报文的发送间隔】mqtt.c ->	[mqtt_timer]函数			//
//															//																		//
//			⑤：向主题"SW_LED"发布"ESP8266_Online"			//	⑥：修改【SNTP服务器设置】user_main.c -> [sntpfn]函数				//
//															//																		//
//			⑥：根据接收到"SW_LED"主题的消息，控制LED亮灭	//	⑦：注释【遗嘱设置】user_main.c -> [user_init]函数					//
//															//																		//
//			⑦：每隔一分钟，向MQTT服务端发送【心跳】		//	⑧：添加【MQTT消息控制LED亮/灭】user_main.c -> [mqttDataCb]函数		//
//															//																		//
//	版本：	V1.1											//																		//
//															//																		//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// 头文件
//==============================
#include "ets_sys.h"
#include "uart.h"
#include "oled.h"  		// OLED
#include "dht11.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "sntp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//==============================
u32  MainLen=0;	//全局通用变量
unsigned char MainBuffer[1460];//缓存数据,全局通用
u8 C_Read_DHT11 = 0;				// 读取DHT11计时
os_timer_t OS_Timer_SNTP;			// 定时器_SNTP
u8 C_LED_Flash = 0;					// LED闪烁计次


// 类型定义
//=================================
typedef unsigned long 		u32_t;
//=================================
os_timer_t OS_Timer_1;		// 软件定时器

// 毫秒延时函数
//===========================================
void ICACHE_FLASH_ATTR delay_ms(u32 C_time)
{	for(;C_time>0;C_time--)
	os_delay_us(1000);
}

// 全局变量
//============================================================================
MQTT_Client mqttClient;			// MQTT客户端_结构体【此变量非常重要】

static ETSTimer OS_Timer_CS;				// 自定义定时器
int count = 0;

// 软件定时器初始化(ms毫秒)
//==========================================================================================

void csfn()
{
	if(DHT11_Read_Data_Complete() == 0)		// 读取DHT11温湿度值
	{
		//-------------------------------------------------
		// DHT11_Data_Array[0] == 湿度_整数_部分
		// DHT11_Data_Array[1] == 湿度_小数_部分
		// DHT11_Data_Array[2] == 温度_整数_部分
		// DHT11_Data_Array[3] == 温度_小数_部分
		// DHT11_Data_Array[4] == 校验字节
		// DHT11_Data_Array[5] == 【1:温度>=0】【0:温度<0】
		//-------------------------------------------------
		// OLED显示温湿度
		//---------------------------------------------------------------------------------
		DHT11_NUM_Char();	// DHT11数据值转成字符串
	}
	u8 C_Str = 0;				// 字符串字节计数

	char A_Str_Data[20] = {0};	// 【"日期"】字符串数组

	char *T_A_Str_Data = A_Str_Data;	// 缓存数组指针

	char A_Str_Clock[10] = {0};	// 【"时间"】字符串数组

	char * Str_Head_Week;		// 【"星期"】字符串首地址

	char * Str_Head_Month;		// 【"月份"】字符串首地址

	char * Str_Head_Day;		// 【"日数"】字符串首地址

	char * Str_Head_Clock;		// 【"时钟"】字符串首地址

	char * Str_Head_Year;		// 【"年份"】字符串首地址

	//------------------------------------------------------


	uint32	TimeStamp;		// 时间戳

	char * Str_RealTime;	// 实际时间的字符串


	// 查询当前距离基准时间(1970.01.01 00:00:00 GMT+8)的时间戳(单位:秒)
	//-----------------------------------------------------------------
	TimeStamp = sntp_get_current_timestamp();

	if(TimeStamp)		// 判断是否获取到偏移时间
	{
		//os_timer_disarm(&OS_Timer_SNTP);	// 关闭SNTP定时器

		// 查询实际时间(GMT+8):东八区(北京时间)
		//--------------------------------------------
		Str_RealTime = sntp_get_real_time(TimeStamp);

		// 时间字符串整理，OLED显示【"日期"】、【"时间"】字符串
		//…………………………………………………………………………………………………………………

		// 【"年份" + ' '】填入日期数组
		//---------------------------------------------------------------------------------
		Str_Head_Year = Str_RealTime;	// 设置起始地址

		while( *Str_Head_Year )		// 找到【"实际时间"】字符串的结束字符'\0'
			Str_Head_Year ++ ;

		// 【注：API返回的实际时间字符串，最后还有一个换行符，所以这里 -5】
		//-----------------------------------------------------------------
		Str_Head_Year -= 5 ;			// 获取【"年份"】字符串的首地址

		T_A_Str_Data[4] = ' ' ;
		os_memcpy(T_A_Str_Data, Str_Head_Year, 4);		// 【"年份" + ' '】填入日期数组

		T_A_Str_Data += 5;				// 指向【"年份" + ' '】字符串的后面的地址
		//---------------------------------------------------------------------------------

		// 获取【日期】字符串的首地址
		//---------------------------------------------------------------------------------
		Str_Head_Week 	= Str_RealTime;							// "星期" 字符串的首地址
		Str_Head_Month = os_strstr(Str_Head_Week,	" ") + 1;	// "月份" 字符串的首地址
		Str_Head_Day 	= os_strstr(Str_Head_Month,	" ") + 1;	// "日数" 字符串的首地址
		Str_Head_Clock = os_strstr(Str_Head_Day,	" ") + 1;	// "时钟" 字符串的首地址


		// 【"月份" + ' '】填入日期数组
		//---------------------------------------------------------------------------------
		C_Str = Str_Head_Day - Str_Head_Month;				// 【"月份" + ' '】的字节数

		os_memcpy(T_A_Str_Data, Str_Head_Month, C_Str);	// 【"月份" + ' '】填入日期数组

		T_A_Str_Data += C_Str;		// 指向【"月份" + ' '】字符串的后面的地址


		// 【"日数" + ' '】填入日期数组
		//---------------------------------------------------------------------------------
		C_Str = Str_Head_Clock - Str_Head_Day;				// 【"日数" + ' '】的字节数

		os_memcpy(T_A_Str_Data, Str_Head_Day, C_Str);		// 【"日数" + ' '】填入日期数组

		T_A_Str_Data += C_Str;		// 指向【"日数" + ' '】字符串的后面的地址


		// 【"星期" + ' '】填入日期数组
		//---------------------------------------------------------------------------------
		C_Str = Str_Head_Month - Str_Head_Week - 1;		// 【"星期"】的字节数

		os_memcpy(T_A_Str_Data, Str_Head_Week, C_Str);		// 【"星期"】填入日期数组

		T_A_Str_Data += C_Str;		// 指向【"星期"】字符串的后面的地址


		// OLED显示【"日期"】、【"时钟"】字符串
		//---------------------------------------------------------------------------------
		*T_A_Str_Data = '\0';		// 【"日期"】字符串后面添加'\0'

		OLED_ShowString(0,0,A_Str_Data);		// OLED显示日期



		os_memcpy(A_Str_Clock, Str_Head_Clock, 8);		// 【"时钟"】字符串填入时钟数组
		A_Str_Clock[8] = '\0';

		OLED_ShowString(64,2,A_Str_Clock);		// OLED显示时间



		//…………………………………………………………………………………………………………………
	}
}

void ICACHE_FLASH_ATTR OS_Timer_1_cb()
{
	if(&mqttClient != NULL){
		int len = strlen("{\"method\":\"thing.event.property.post\",\"id\":1111,\
						          \"params\":{\"temp\":99.9,\"humi\":99.9},\"version\":\"1.0\"}");

		char result[len] ;
		result [0] = '\0';
		strcat(result,"{\"method\":\"thing.event.property.post\",\"id\":1111,\
						  	  			\"params\":{\"temp\":");
		char aa[strlen("99.9")];
		aa [0] = '\0';
		int i=0;
		for( i;i < strlen(DHT11_Data_Char[1]);i++){
			if(DHT11_Data_Char[1][i] ==  '.'){
				aa[i] = DHT11_Data_Char[1][i];
				aa[i+1] = DHT11_Data_Char[1][i+1];
				aa[i + 2] = '\0';
				break;
			}else{
				aa[i] = DHT11_Data_Char[1][i];
			}
		}

		strcat(result,aa);
		strcat(result,",\"humi\":");

		char bb[strlen("99.9")];
		bb [0] = '\0';
		int j=0;
		for( j;j < strlen(DHT11_Data_Char[0]);j++){
			if(DHT11_Data_Char[0][j] == '.' ){
				bb[j] = DHT11_Data_Char[0][j];
				bb[j+1] = DHT11_Data_Char[0][j+1];
				bb[j + 2] = '\0';
				break;
			}else{
				bb[j] = DHT11_Data_Char[0][j];
			}
		}

		strcat(result,bb);
		strcat(result,"},\"version\":\"1.0\"}");

		MainLen = os_sprintf((char*)MainBuffer,result);
		MQTT_Publish(&mqttClient, "/sys/a1z0odVg8Qs/8266-1/thing/event/property/post", MainBuffer, MainLen, 0, 1);//发布消息

	}
	OLED_ShowString(64,4,DHT11_Data_Char[1]);	// DHT11_Data_Char[0] == 【温度字符串】
	OLED_ShowString(64,6,DHT11_Data_Char[0]);	// DHT11_Data_Char[1] == 【湿度字符串】

}

//=============================================================================
void ICACHE_FLASH_ATTR OS_Timer_1_Init_JX(u32 time_ms, u8 time_repetitive)
{

	os_timer_disarm(&OS_Timer_1);	// 关闭定时器
	os_timer_setfn(&OS_Timer_1,(os_timer_func_t *)OS_Timer_1_cb, NULL);	// 设置定时器
	os_timer_arm(&OS_Timer_1, time_ms, time_repetitive);  // 使能定时器
}

//===================================================================================================

// SNTP定时初始化
//=============================================================================
void ICACHE_FLASH_ATTR OS_Timer_SNTP_Init_JX(u32 time_ms, u8 time_repetitive)
{
	os_timer_disarm(&OS_Timer_SNTP);
	os_timer_setfn(&OS_Timer_SNTP,(os_timer_func_t *)csfn,NULL);
	os_timer_arm(&OS_Timer_SNTP, time_ms, time_repetitive);
}




static ETSTimer sntp_timer;		// SNTP定时器
//============================================================================

// WIFI连接状态改变：参数 = wifiStatus
//============================================================================
void wifiConnectCb(uint8_t status)
{
	// 成功获取到IP地址
	//---------------------------------------------------------------------
	if(status == STATION_GOT_IP)
	{
		ip_addr_t * addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));

		// 在官方例程的基础上，增加2个备用服务器
		//---------------------------------------------------------------
		sntp_setservername(0, "us.pool.ntp.org");	// 服务器_0【域名】
		sntp_setservername(1, "ntp.sjtu.edu.cn");	// 服务器_1【域名】

		ipaddr_aton("210.72.145.44", addr);	// 点分十进制 => 32位二进制
		sntp_setserver(2, addr);					// 服务器_2【IP地址】
		os_free(addr);								// 释放addr

		sntp_init();	// SNTP初始化
		MQTT_Connect(&mqttClient);		// 开始MQTT连接

		// 设置SNTP定时器[sntp_timer]
		//-----------------------------------------------------------

		OS_Timer_SNTP_Init_JX(1000,1);				// 1秒重复定时(SNTP)  时钟
		OS_Timer_1_Init_JX(5000,1);		// 5秒定时(重复)  读取温湿度上传

	}

	// IP地址获取失败
	//----------------------------------------------------------------
	else
	{
		MQTT_Disconnect(&mqttClient);	// WIFI连接出错，TCP断开连接
	}
}
//============================================================================


// MQTT已成功连接：ESP8266发送【CONNECT】，并接收到【CONNACK】
//============================================================================
void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;	// 获取mqttClient指针

	INFO("MQTT: Connected\r\n");
	//----------------------------------------------------------------
	OLED_ShowString(0,2,"Clock =         ");	// Clock：时钟
	OLED_ShowString(0,4,"Temp  =         ");	// Temperature：温度
	OLED_ShowString(0,6,"Humid =         ");	// Humidity：湿度

	//----------------------------------------------------------------

	// 【参数2：主题过滤器 / 参数3：订阅Qos】
	//-----------------------------------------------------------------
	MQTT_Subscribe(client, "/a1z0odVg8Qs/8266-1/user/SW_LED", 0);	// 订阅主题"SW_LED"，QoS=0
	//	MQTT_Subscribe(client, "SW_LED", 1);
	//	MQTT_Subscribe(client, "SW_LED", 2);



	// 【参数2：主题名 / 参数3：发布消息的有效载荷 / 参数4：有效载荷长度 / 参数5：发布Qos / 参数6：Retain】
	//-----------------------------------------------------------------------------------------------------------------------------------------
	//MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 0, 0);	// 向主题"SW_LED"发布"ESP8266_Online"，Qos=0、retain=0
	//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 1, 0);
	//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 2, 0);
}
//============================================================================

// MQTT成功断开连接
//============================================================================
void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}
//============================================================================

// MQTT成功发布消息
//============================================================================
void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}
//============================================================================

// 【接收MQTT的[PUBLISH]数据】函数		【参数1：主题 / 参数2：主题长度 / 参数3：有效载荷 / 参数4：有效载荷长度】
//===============================================================================================================
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1);		// 申请【主题】空间
	char *dataBuf  = (char*)os_zalloc(data_len+1);		// 申请【有效载荷】空间


	MQTT_Client* client = (MQTT_Client*)args;	// 获取MQTT_Client指针


	os_memcpy(topicBuf, topic, topic_len);	// 缓存主题
	topicBuf[topic_len] = 0;				// 最后添'\0'

	os_memcpy(dataBuf, data, data_len);		// 缓存有效载荷
	dataBuf[data_len] = 0;					// 最后添'\0'

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);	// 串口打印【主题】【有效载荷】
	INFO("Topic_len = %d, Data_len = %d\r\n", topic_len, data_len);	// 串口打印【主题长度】【有效载荷长度】


	// 【技小新】添加
	//########################################################################################
	// 根据接收到的主题名/有效载荷，控制LED的亮/灭
	//-----------------------------------------------------------------------------------
	if( os_strncmp(topicBuf,"/a1z0odVg8Qs/8266-1/user/SW_LED",topic_len) == 0 )			// 主题 == "SW_LED"
	{
		if( os_strncmp(dataBuf,"{\"SW_LED\":\"ON\"}",data_len) == 0 )
		{
			GPIO_OUTPUT_SET(GPIO_ID_PIN(4),0);
		}
		else if ( os_strncmp(dataBuf,"{\"SW_LED\":\"OFF\"}",data_len) == 0 )
		{
			GPIO_OUTPUT_SET(GPIO_ID_PIN(4),1);
		}
	}
	//########################################################################################


	os_free(topicBuf);	// 释放【主题】空间
	os_free(dataBuf);	// 释放【有效载荷】空间
}
//===============================================================================================================

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
	case FLASH_SIZE_4M_MAP_256_256:
		rf_cal_sec = 128 - 5;
		break;

	case FLASH_SIZE_8M_MAP_512_512:
		rf_cal_sec = 256 - 5;
		break;

	case FLASH_SIZE_16M_MAP_512_512:
	case FLASH_SIZE_16M_MAP_1024_1024:
		rf_cal_sec = 512 - 5;
		break;

	case FLASH_SIZE_32M_MAP_512_512:
	case FLASH_SIZE_32M_MAP_1024_1024:
		rf_cal_sec = 1024 - 5;
		break;

	case FLASH_SIZE_64M_MAP_1024_1024:
		rf_cal_sec = 2048 - 5;
		break;
	case FLASH_SIZE_128M_MAP_1024_1024:
		rf_cal_sec = 4096 - 5;
		break;
	default:
		rf_cal_sec = 0;
		break;
	}

	return rf_cal_sec;
}

// user_init：entry of user application, init user function here
//===================================================================================================================
void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);	// 串口波特率设为115200
	os_delay_us(60000);

	OLED_Init();
	OLED_ShowString(0,0,"Connecting...");
	//【技小新】添加
	//###########################################################################
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U,	FUNC_GPIO4);	// GPIO4输出高	#
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4),1);						// LED初始化	#
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,	FUNC_GPIO12);	// GPIO4输出高	#
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12),1);						// LED初始化	#
	//###########################################################################


	CFG_Load();	// 加载/更新系统参数【WIFI参数、MQTT参数】


	// 网络连接参数赋值：服务端域名【mqtt_test_jx.mqtt.iot.gz.baidubce.com】、网络连接端口【1883】、安全类型【0：NO_TLS】
	//-------------------------------------------------------------------------------------------------------------------
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);

	// MQTT连接参数赋值：客户端标识符【..】、MQTT用户名【..】、MQTT密钥【..】、保持连接时长【120s】、清除会话【1：clean_session】
	//----------------------------------------------------------------------------------------------------------------------------
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);

	// 设置遗嘱参数(如果云端没有对应的遗嘱主题，则MQTT连接会被拒绝)
	//--------------------------------------------------------------
	//	MQTT_InitLWT(&mqttClient, "Will", "ESP8266_offline", 0, 0);


	// 设置MQTT相关函数
	//--------------------------------------------------------------------------------------------------
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);			// 设置【MQTT成功连接】函数的另一种调用方式
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);	// 设置【MQTT成功断开】函数的另一种调用方式
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);			// 设置【MQTT成功发布】函数的另一种调用方式
	MQTT_OnData(&mqttClient, mqttDataCb);					// 设置【接收MQTT数据】函数的另一种调用方式


	// 连接WIFI：SSID[..]、PASSWORD[..]、WIFI连接成功函数[wifiConnectCb]
	//--------------------------------------------------------------------------
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);


	INFO("\r\nSystem  started ...\r\n");
}
//===================================================================================================================
