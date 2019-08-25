/*
 * main.c
 *
 *  Created on: Aug 12, 2019
 *      Author: cedrus
 */

#include "User_include.h"

#define APP_IPV4_HOST_ADDR "192.168.117.103"
#define APP_SERVER_PORT 23

NetInterface *interface;
DhcpClientSettings dhcpClientSettings;
DhcpClientContext dhcpClientContext;

/**
 * @brief TCP client test routine
 * @return Error code
 **/

error_t tcpClientTest(void)
{
	error_t error;
	size_t length;
	IpAddr ipAddr;
	Socket *socket;
	static char_t buffer[256];

	//Debug message
	TRACE_INFO("\r\n\r\nResolving server name...\r\n");

	//Create a new socket to handle the request
	socket = socketOpen(SOCKET_TYPE_STREAM, SOCKET_IP_PROTO_TCP);
	//Any error to report?
	if (!socket)
	{
		//Debug message
		TRACE_INFO("Failed to open socket!\r\n");
		//Exit immediately
		return ERROR_OPEN_FAILED;
	}

	//Start of exception handling block
	do
	{
		ipStringToAddr(APP_IPV4_HOST_ADDR, &ipAddr);
		//Debug message
		TRACE_INFO("Connecting to HTTP server %s\r\n",
				ipAddrToString(&ipAddr, NULL));

		//Connect to the HTTP server
		error = socketConnect(socket, &ipAddr, APP_SERVER_PORT);
		//Any error to report?
		if (error)
			break;

		//Debug message
		TRACE_INFO("Successful connection\r\n");

		while (1)
		{
			//Read the header line by line
			error = socketReceive(socket, buffer, sizeof(buffer) - 1, &length,
					SOCKET_FLAG_DONT_WAIT);
			//End of stream?
			if (!error)
			//			break;
			{
				//Properly terminate the string with a NULL character
				buffer[length] = '\0';
				error = socketSend(socket, buffer, length, NULL,
						SOCKET_FLAG_WAIT_ACK);
				break;
			}
			osDelayTask(100);
		}

		//End of exception handling block
	} while (0);

	//Close the connection
	socketClose(socket);
	//Debug message
	TRACE_INFO("\r\nConnection closed...\r\n");

	//Return status code
	return error;
}

static void mainTask(void)
{
	while (1)
	{
		GPIO_SetBits(GPIOE, GPIO_Pin_3);
		osDelayTask(5000);
		GPIO_ResetBits(GPIOE, GPIO_Pin_3);
		osDelayTask(5000);
	}
}

/**
 * @brief User task
 **/

void userTask(void *param)
{
	//Endless loop
	while (1)
	{
		//HTTP client test routine
		tcpClientTest();
		//Loop delay
		osDelayTask(100);
	}
}

int main(void)
{
	error_t error;
	MacAddr macAddr;
	OsTask *mainTaskHandle;

//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	LED_Init();

	//Initialize kernel
	osInitKernel();

	//Initialize TCP/IP stack
	error = netInit();
	//Any error to report?
	if (error)
	{
		return ERROR;
	}

	//Configure the first Ethernet interface
	interface = &netInterface[0];

	//Set interface name
	netSetInterfaceName(interface, "eth0");
	//Select the relevant network adapter
	netSetDriver(interface, &stm32f4xxEthDriver);
	netSetPhyDriver(interface, &dp83848PhyDriver);
	//Set host MAC address
	macStringToAddr("00-AB-CD-EF-02-07", &macAddr);
	netSetMacAddr(interface, &macAddr);

	//Initialize network interface
	error = netConfigInterface(interface);
	//Failed to configure the interface?
	if (error)
	{
		return ERROR;
	}

	//DHCP initialization or manual configuration (IP address,
	//subnet mask, default gateway and DNS servers) goes here
#if (IPV4_SUPPORT == ENABLED)
#if (APP_USE_DHCP_CLIENT == ENABLED)

	//Get default settings
	dhcpClientGetDefaultSettings(&dhcpClientSettings);
	//Set the network interface to be configured by DHCP
	dhcpClientSettings.interface = interface;
	//Disable rapid commit option
	dhcpClientSettings.rapidCommit = FALSE;
	//DHCP client initialization
	error = dhcpClientInit(&dhcpClientContext, &dhcpClientSettings);
	//Failed to initialize DHCP client?
	if (error)
	{
		//Debug message
		TRACE_ERROR("Failed to initialize DHCP client!\r\n");
		return ERROR;
	}

	//Start DHCP client
	error = dhcpClientStart(&dhcpClientContext);
	//Failed to start DHCP client?
	if (error)
	{
		//Debug message
		TRACE_ERROR("Failed to start DHCP client!\r\n");
		return ERROR;
	}
#endif
#endif

	//Create a task to handle main
	mainTaskHandle = osCreateTask("LED", (OsTaskCode) mainTask, NULL,
	NET_TASK_STACK_SIZE, NET_TASK_PRIORITY);

	//Create user task
	mainTaskHandle = osCreateTask("User Task", userTask, NULL, 500,
	OS_TASK_PRIORITY_NORMAL);
	//Failed to create the task?
	if (mainTaskHandle == OS_INVALID_HANDLE)
	{
		//Debug message
		TRACE_ERROR("Failed to create task!\r\n");
	}

	//Unable to create the task?
	if (mainTaskHandle == OS_INVALID_HANDLE)
		return ERROR_OUT_OF_RESOURCES;

	//The TCP/IP stack is now running
	osStartKernel();

	return 0;
}
