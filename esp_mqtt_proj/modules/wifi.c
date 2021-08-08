/*
 * wifi.c
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */
#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"

static ETSTimer WiFiLinker;		// ��ʱ����WIFI����

WifiCallback wifiCb = NULL;		// WIFI���ӳɹ��ص�����

static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;	// ��ʼ��WIFI״̬��STAδ����WIFI


// ��ʱ���������IP��ȡ���
//==============================================================================
static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;

	os_timer_disarm(&WiFiLinker);	// �ر�WiFiLinker��ʱ��

	wifi_get_ip_info(STATION_IF, &ipConfig);		// ��ȡIP��ַ

	wifiStatus = wifi_station_get_connect_status();	// ��ȡ����״̬


	// ��ȡ��IP��ַ
	//-------------------------------------------------------------------------
	if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
	{
		// ��ȡIP��ÿ2����һ��WIFI���ӵ���ȷ�ԡ���ֹWIFI���ߵ������
		//------------------------------------------------------------------
		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 2000, 0);
	}
	//-------------------------------------------------------------------------


	// δ��ȡ��IP��ַ
	//-------------------------------------------------------------------------
	else
	{
		if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
		{
			INFO("STATION_WRONG_PASSWORD\r\n");	// �������

			wifi_station_connect();
		}
		else if(wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
		{
			INFO("STATION_NO_AP_FOUND\r\n");	// δ���ֶ�ӦAP

			wifi_station_connect();
		}
		else if(wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
		{
			INFO("STATION_CONNECT_FAIL\r\n");	// ����ʧ��

			wifi_station_connect();
		}
		else
		{
			INFO("STATION99999_IDLE\r\n");
		}

		// �ٴο�����ʱ��
		//------------------------------------------------------------------
		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 500, 0);		// 500Ms��ʱ
	}
	//-------------------------------------------------------------------------

	// ���WIFI״̬�ı䣬�����[wifiConnectCb]����
	//-------------------------------------------------------------------------
	if(wifiStatus != lastWifiStatus)
	{
		lastWifiStatus = wifiStatus;	// WIFI״̬����

		if(wifiCb)				// �ж��Ƿ�������[wifiConnectCb]����
		wifiCb(wifiStatus);		// wifiCb(wifiStatus)=wifiConnectCb(wifiStatus)
	}
}
//==============================================================================


// ����WIFI��SSID��...����PASSWORD��...����WIFI���ӳɹ��ص�������wifiConnectCb��
//====================================================================================================================
void ICACHE_FLASH_ATTR WIFI_Connect(uint8_t* ssid, uint8_t* pass, WifiCallback cb)
{
	struct station_config stationConf;

	INFO("WIFI_INIT\r\n");

	wifi_set_opmode_current(STATION_MODE);		// ����ESP8266ΪSTAģʽ

	//wifi_station_set_auto_connect(FALSE);		// �ϵ粻�Զ������Ѽ�¼��AP(��ע�ͣ������ϵ��Զ������Ѽ�¼��AP(Ĭ��))


	//--------------------------------------------------------------------------------------
	wifiCb = cb;	// ��������ֵ��wifiCb������Ϊ������ʹ�ã�wifiCb(..) == wifiConnectCb(..)


	// ����STA����
	//--------------------------------------------------------------------------
	os_memset(&stationConf, 0, sizeof(struct station_config));	// STA��Ϣ = 0
	os_sprintf(stationConf.ssid, "%s", ssid);					// SSID��ֵ
	os_sprintf(stationConf.password, "%s", pass);				// ���븳ֵ
	wifi_station_set_config_current(&stationConf);	// ����STA����


	// ����WiFiLinker��ʱ��
	//-------------------------------------------------------------------------------------------------------
	os_timer_disarm(&WiFiLinker);	// ��ʱ����WIFI����
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);	// wifi_check_ip�����IP��ȡ���
	os_timer_arm(&WiFiLinker, 1000, 0);		// 1�붨ʱ(1��)

	//wifi_station_set_auto_connect(TRUE);	// �ϵ��Զ������Ѽ�¼AP


	wifi_station_connect();		// ESP8266����WIFI
}
//====================================================================================================================
