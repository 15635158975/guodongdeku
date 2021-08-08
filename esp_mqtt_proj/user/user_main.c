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
// ���̣�	MQTT_JX											//	ע���ڡ�esp_mqtt_proj���������޸�									//
//															//																		//
// ƽ̨��	�����µ��ӡ������������� ESP8266 V1.0			//	�٣���ӡ���ʵ��ע�͡�������˵�ˣ�˵���˶����ᣡ����				//
//															//																		//
// ���ܣ�	�٣�����MQTT��ز���							//	�ڣ��޸ġ�MQTT�������顿config.h -> device_id/mqtt_host/mqtt_pass	//
//															//																		//
//			�ڣ���MQTT����ˣ�������������(TCP)				//	�ۣ��޸ġ�MQTT_CLIENT_ID�궨�塿mqtt_config.h -> MQTT_CLIENT_ID		//
//															//																		//
//			�ۣ�����/���͡�CONNECT�����ģ�����MQTT�����	//	�ܣ��޸ġ�PROTOCOL_NAMEv31�꡿mqtt_config.h -> PROTOCOL_NAMEv311	//
//															//																		//
//			�ܣ���������"SW_LED"							//	�ݣ��޸ġ��������ĵķ��ͼ����mqtt.c ->	[mqtt_timer]����			//
//															//																		//
//			�ݣ�������"SW_LED"����"ESP8266_Online"			//	�ޣ��޸ġ�SNTP���������á�user_main.c -> [sntpfn]����				//
//															//																		//
//			�ޣ����ݽ��յ�"SW_LED"�������Ϣ������LED����	//	�ߣ�ע�͡��������á�user_main.c -> [user_init]����					//
//															//																		//
//			�ߣ�ÿ��һ���ӣ���MQTT����˷��͡�������		//	�ࣺ��ӡ�MQTT��Ϣ����LED��/��user_main.c -> [mqttDataCb]����		//
//															//																		//
//	�汾��	V1.1											//																		//
//															//																		//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// ͷ�ļ�
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
u32  MainLen=0;	//ȫ��ͨ�ñ���
unsigned char MainBuffer[1460];//��������,ȫ��ͨ��
u8 C_Read_DHT11 = 0;				// ��ȡDHT11��ʱ
os_timer_t OS_Timer_SNTP;			// ��ʱ��_SNTP
u8 C_LED_Flash = 0;					// LED��˸�ƴ�


// ���Ͷ���
//=================================
typedef unsigned long 		u32_t;
//=================================
os_timer_t OS_Timer_1;		// �����ʱ��

// ������ʱ����
//===========================================
void ICACHE_FLASH_ATTR delay_ms(u32 C_time)
{	for(;C_time>0;C_time--)
	os_delay_us(1000);
}

// ȫ�ֱ���
//============================================================================
MQTT_Client mqttClient;			// MQTT�ͻ���_�ṹ�塾�˱����ǳ���Ҫ��

static ETSTimer OS_Timer_CS;				// �Զ��嶨ʱ��
int count = 0;

// �����ʱ����ʼ��(ms����)
//==========================================================================================

void csfn()
{
	if(DHT11_Read_Data_Complete() == 0)		// ��ȡDHT11��ʪ��ֵ
	{
		//-------------------------------------------------
		// DHT11_Data_Array[0] == ʪ��_����_����
		// DHT11_Data_Array[1] == ʪ��_С��_����
		// DHT11_Data_Array[2] == �¶�_����_����
		// DHT11_Data_Array[3] == �¶�_С��_����
		// DHT11_Data_Array[4] == У���ֽ�
		// DHT11_Data_Array[5] == ��1:�¶�>=0����0:�¶�<0��
		//-------------------------------------------------
		// OLED��ʾ��ʪ��
		//---------------------------------------------------------------------------------
		DHT11_NUM_Char();	// DHT11����ֵת���ַ���
	}
	u8 C_Str = 0;				// �ַ����ֽڼ���

	char A_Str_Data[20] = {0};	// ��"����"���ַ�������

	char *T_A_Str_Data = A_Str_Data;	// ��������ָ��

	char A_Str_Clock[10] = {0};	// ��"ʱ��"���ַ�������

	char * Str_Head_Week;		// ��"����"���ַ����׵�ַ

	char * Str_Head_Month;		// ��"�·�"���ַ����׵�ַ

	char * Str_Head_Day;		// ��"����"���ַ����׵�ַ

	char * Str_Head_Clock;		// ��"ʱ��"���ַ����׵�ַ

	char * Str_Head_Year;		// ��"���"���ַ����׵�ַ

	//------------------------------------------------------


	uint32	TimeStamp;		// ʱ���

	char * Str_RealTime;	// ʵ��ʱ����ַ���


	// ��ѯ��ǰ�����׼ʱ��(1970.01.01 00:00:00 GMT+8)��ʱ���(��λ:��)
	//-----------------------------------------------------------------
	TimeStamp = sntp_get_current_timestamp();

	if(TimeStamp)		// �ж��Ƿ��ȡ��ƫ��ʱ��
	{
		//os_timer_disarm(&OS_Timer_SNTP);	// �ر�SNTP��ʱ��

		// ��ѯʵ��ʱ��(GMT+8):������(����ʱ��)
		//--------------------------------------------
		Str_RealTime = sntp_get_real_time(TimeStamp);

		// ʱ���ַ�������OLED��ʾ��"����"������"ʱ��"���ַ���
		//��������������������������������������������������������������������������������������

		// ��"���" + ' '��������������
		//---------------------------------------------------------------------------------
		Str_Head_Year = Str_RealTime;	// ������ʼ��ַ

		while( *Str_Head_Year )		// �ҵ���"ʵ��ʱ��"���ַ����Ľ����ַ�'\0'
			Str_Head_Year ++ ;

		// ��ע��API���ص�ʵ��ʱ���ַ����������һ�����з����������� -5��
		//-----------------------------------------------------------------
		Str_Head_Year -= 5 ;			// ��ȡ��"���"���ַ������׵�ַ

		T_A_Str_Data[4] = ' ' ;
		os_memcpy(T_A_Str_Data, Str_Head_Year, 4);		// ��"���" + ' '��������������

		T_A_Str_Data += 5;				// ָ��"���" + ' '���ַ����ĺ���ĵ�ַ
		//---------------------------------------------------------------------------------

		// ��ȡ�����ڡ��ַ������׵�ַ
		//---------------------------------------------------------------------------------
		Str_Head_Week 	= Str_RealTime;							// "����" �ַ������׵�ַ
		Str_Head_Month = os_strstr(Str_Head_Week,	" ") + 1;	// "�·�" �ַ������׵�ַ
		Str_Head_Day 	= os_strstr(Str_Head_Month,	" ") + 1;	// "����" �ַ������׵�ַ
		Str_Head_Clock = os_strstr(Str_Head_Day,	" ") + 1;	// "ʱ��" �ַ������׵�ַ


		// ��"�·�" + ' '��������������
		//---------------------------------------------------------------------------------
		C_Str = Str_Head_Day - Str_Head_Month;				// ��"�·�" + ' '�����ֽ���

		os_memcpy(T_A_Str_Data, Str_Head_Month, C_Str);	// ��"�·�" + ' '��������������

		T_A_Str_Data += C_Str;		// ָ��"�·�" + ' '���ַ����ĺ���ĵ�ַ


		// ��"����" + ' '��������������
		//---------------------------------------------------------------------------------
		C_Str = Str_Head_Clock - Str_Head_Day;				// ��"����" + ' '�����ֽ���

		os_memcpy(T_A_Str_Data, Str_Head_Day, C_Str);		// ��"����" + ' '��������������

		T_A_Str_Data += C_Str;		// ָ��"����" + ' '���ַ����ĺ���ĵ�ַ


		// ��"����" + ' '��������������
		//---------------------------------------------------------------------------------
		C_Str = Str_Head_Month - Str_Head_Week - 1;		// ��"����"�����ֽ���

		os_memcpy(T_A_Str_Data, Str_Head_Week, C_Str);		// ��"����"��������������

		T_A_Str_Data += C_Str;		// ָ��"����"���ַ����ĺ���ĵ�ַ


		// OLED��ʾ��"����"������"ʱ��"���ַ���
		//---------------------------------------------------------------------------------
		*T_A_Str_Data = '\0';		// ��"����"���ַ����������'\0'

		OLED_ShowString(0,0,A_Str_Data);		// OLED��ʾ����



		os_memcpy(A_Str_Clock, Str_Head_Clock, 8);		// ��"ʱ��"���ַ�������ʱ������
		A_Str_Clock[8] = '\0';

		OLED_ShowString(64,2,A_Str_Clock);		// OLED��ʾʱ��



		//��������������������������������������������������������������������������������������
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
		MQTT_Publish(&mqttClient, "/sys/a1z0odVg8Qs/8266-1/thing/event/property/post", MainBuffer, MainLen, 0, 1);//������Ϣ

	}
	OLED_ShowString(64,4,DHT11_Data_Char[1]);	// DHT11_Data_Char[0] == ���¶��ַ�����
	OLED_ShowString(64,6,DHT11_Data_Char[0]);	// DHT11_Data_Char[1] == ��ʪ���ַ�����

}

//=============================================================================
void ICACHE_FLASH_ATTR OS_Timer_1_Init_JX(u32 time_ms, u8 time_repetitive)
{

	os_timer_disarm(&OS_Timer_1);	// �رն�ʱ��
	os_timer_setfn(&OS_Timer_1,(os_timer_func_t *)OS_Timer_1_cb, NULL);	// ���ö�ʱ��
	os_timer_arm(&OS_Timer_1, time_ms, time_repetitive);  // ʹ�ܶ�ʱ��
}

//===================================================================================================

// SNTP��ʱ��ʼ��
//=============================================================================
void ICACHE_FLASH_ATTR OS_Timer_SNTP_Init_JX(u32 time_ms, u8 time_repetitive)
{
	os_timer_disarm(&OS_Timer_SNTP);
	os_timer_setfn(&OS_Timer_SNTP,(os_timer_func_t *)csfn,NULL);
	os_timer_arm(&OS_Timer_SNTP, time_ms, time_repetitive);
}




static ETSTimer sntp_timer;		// SNTP��ʱ��
//============================================================================

// WIFI����״̬�ı䣺���� = wifiStatus
//============================================================================
void wifiConnectCb(uint8_t status)
{
	// �ɹ���ȡ��IP��ַ
	//---------------------------------------------------------------------
	if(status == STATION_GOT_IP)
	{
		ip_addr_t * addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));

		// �ڹٷ����̵Ļ����ϣ�����2�����÷�����
		//---------------------------------------------------------------
		sntp_setservername(0, "us.pool.ntp.org");	// ������_0��������
		sntp_setservername(1, "ntp.sjtu.edu.cn");	// ������_1��������

		ipaddr_aton("210.72.145.44", addr);	// ���ʮ���� => 32λ������
		sntp_setserver(2, addr);					// ������_2��IP��ַ��
		os_free(addr);								// �ͷ�addr

		sntp_init();	// SNTP��ʼ��
		MQTT_Connect(&mqttClient);		// ��ʼMQTT����

		// ����SNTP��ʱ��[sntp_timer]
		//-----------------------------------------------------------

		OS_Timer_SNTP_Init_JX(1000,1);				// 1���ظ���ʱ(SNTP)  ʱ��
		OS_Timer_1_Init_JX(5000,1);		// 5�붨ʱ(�ظ�)  ��ȡ��ʪ���ϴ�

	}

	// IP��ַ��ȡʧ��
	//----------------------------------------------------------------
	else
	{
		MQTT_Disconnect(&mqttClient);	// WIFI���ӳ���TCP�Ͽ�����
	}
}
//============================================================================


// MQTT�ѳɹ����ӣ�ESP8266���͡�CONNECT���������յ���CONNACK��
//============================================================================
void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;	// ��ȡmqttClientָ��

	INFO("MQTT: Connected\r\n");
	//----------------------------------------------------------------
	OLED_ShowString(0,2,"Clock =         ");	// Clock��ʱ��
	OLED_ShowString(0,4,"Temp  =         ");	// Temperature���¶�
	OLED_ShowString(0,6,"Humid =         ");	// Humidity��ʪ��

	//----------------------------------------------------------------

	// ������2����������� / ����3������Qos��
	//-----------------------------------------------------------------
	MQTT_Subscribe(client, "/a1z0odVg8Qs/8266-1/user/SW_LED", 0);	// ��������"SW_LED"��QoS=0
	//	MQTT_Subscribe(client, "SW_LED", 1);
	//	MQTT_Subscribe(client, "SW_LED", 2);



	// ������2�������� / ����3��������Ϣ����Ч�غ� / ����4����Ч�غɳ��� / ����5������Qos / ����6��Retain��
	//-----------------------------------------------------------------------------------------------------------------------------------------
	//MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 0, 0);	// ������"SW_LED"����"ESP8266_Online"��Qos=0��retain=0
	//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 1, 0);
	//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 2, 0);
}
//============================================================================

// MQTT�ɹ��Ͽ�����
//============================================================================
void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}
//============================================================================

// MQTT�ɹ�������Ϣ
//============================================================================
void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}
//============================================================================

// ������MQTT��[PUBLISH]���ݡ�����		������1������ / ����2�����ⳤ�� / ����3����Ч�غ� / ����4����Ч�غɳ��ȡ�
//===============================================================================================================
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1);		// ���롾���⡿�ռ�
	char *dataBuf  = (char*)os_zalloc(data_len+1);		// ���롾��Ч�غɡ��ռ�


	MQTT_Client* client = (MQTT_Client*)args;	// ��ȡMQTT_Clientָ��


	os_memcpy(topicBuf, topic, topic_len);	// ��������
	topicBuf[topic_len] = 0;				// �����'\0'

	os_memcpy(dataBuf, data, data_len);		// ������Ч�غ�
	dataBuf[data_len] = 0;					// �����'\0'

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);	// ���ڴ�ӡ�����⡿����Ч�غɡ�
	INFO("Topic_len = %d, Data_len = %d\r\n", topic_len, data_len);	// ���ڴ�ӡ�����ⳤ�ȡ�����Ч�غɳ��ȡ�


	// ����С�¡����
	//########################################################################################
	// ���ݽ��յ���������/��Ч�غɣ�����LED����/��
	//-----------------------------------------------------------------------------------
	if( os_strncmp(topicBuf,"/a1z0odVg8Qs/8266-1/user/SW_LED",topic_len) == 0 )			// ���� == "SW_LED"
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


	os_free(topicBuf);	// �ͷš����⡿�ռ�
	os_free(dataBuf);	// �ͷš���Ч�غɡ��ռ�
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

// user_init��entry of user application, init user function here
//===================================================================================================================
void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);	// ���ڲ�������Ϊ115200
	os_delay_us(60000);

	OLED_Init();
	OLED_ShowString(0,0,"Connecting...");
	//����С�¡����
	//###########################################################################
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U,	FUNC_GPIO4);	// GPIO4�����	#
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4),1);						// LED��ʼ��	#
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,	FUNC_GPIO12);	// GPIO4�����	#
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12),1);						// LED��ʼ��	#
	//###########################################################################


	CFG_Load();	// ����/����ϵͳ������WIFI������MQTT������


	// �������Ӳ�����ֵ�������������mqtt_test_jx.mqtt.iot.gz.baidubce.com�����������Ӷ˿ڡ�1883������ȫ���͡�0��NO_TLS��
	//-------------------------------------------------------------------------------------------------------------------
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);

	// MQTT���Ӳ�����ֵ���ͻ��˱�ʶ����..����MQTT�û�����..����MQTT��Կ��..������������ʱ����120s��������Ự��1��clean_session��
	//----------------------------------------------------------------------------------------------------------------------------
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);

	// ������������(����ƶ�û�ж�Ӧ���������⣬��MQTT���ӻᱻ�ܾ�)
	//--------------------------------------------------------------
	//	MQTT_InitLWT(&mqttClient, "Will", "ESP8266_offline", 0, 0);


	// ����MQTT��غ���
	//--------------------------------------------------------------------------------------------------
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);			// ���á�MQTT�ɹ����ӡ���������һ�ֵ��÷�ʽ
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);	// ���á�MQTT�ɹ��Ͽ�����������һ�ֵ��÷�ʽ
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);			// ���á�MQTT�ɹ���������������һ�ֵ��÷�ʽ
	MQTT_OnData(&mqttClient, mqttDataCb);					// ���á�����MQTT���ݡ���������һ�ֵ��÷�ʽ


	// ����WIFI��SSID[..]��PASSWORD[..]��WIFI���ӳɹ�����[wifiConnectCb]
	//--------------------------------------------------------------------------
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);


	INFO("\r\nSystem  started ...\r\n");
}
//===================================================================================================================
