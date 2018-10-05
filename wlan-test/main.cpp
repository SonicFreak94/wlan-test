#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wlanapi.h>

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
	DWORD negotiated_version = 0;
	HANDLE handle = nullptr;

	HRESULT hr = WlanOpenHandle(WLAN_API_VERSION, nullptr, &negotiated_version, &handle);

	if (FAILED(hr))
	{
		std::cout << "WlanOpenHandle failed with error code: " << hr << std::endl;
		return -1;
	}

	PWLAN_INTERFACE_INFO_LIST interface_list = nullptr;

	hr = WlanEnumInterfaces(handle, nullptr, &interface_list);

	if (FAILED(hr))
	{
		WlanCloseHandle(handle, nullptr);
		std::cout << "WlanEnumInterfaces failed with error code: " << hr << std::endl;
		return -2;
	}

	if (interface_list->dwNumberOfItems == 0)
	{
		WlanFreeMemory(interface_list);
		WlanCloseHandle(handle, nullptr);
		std::cout << "WlanEnumInterfaces returned zero interfaces!" << std::endl;
		return -3;
	}

	// blindly select the first one because reasons
	GUID guid = interface_list->InterfaceInfo[0].InterfaceGuid;

	WlanFreeMemory(interface_list);
	interface_list = nullptr;

	hr = WlanScan(handle, &guid, nullptr, nullptr, nullptr);
	if (FAILED(hr))
	{
		std::cout << "Warning: WlanScan failed with error code " << hr << std::endl;
	}

	// note that WlanGetAvailableNetworkList2 (*2*) does not exist on Windows 7
	// (not that we needed the info provided by it anyway, but LET IT BE KNOWN)
	PWLAN_AVAILABLE_NETWORK_LIST available_networks = nullptr;
	hr = WlanGetAvailableNetworkList(handle, &guid, 0, nullptr, &available_networks);

	if (FAILED(hr))
	{
		std::cout << "WlanGetAvailableNetworkList failed with error code: " << hr << std::endl;
	}
	else
	{
		for (size_t i = 0; i < available_networks->dwNumberOfItems; ++i)
		{
			const auto& network = available_networks->Network[i];

			std::cout << i + 1 << ": ";

			if (network.strProfileName[0] != L'\0')
			{
				std::cout << "[connected] ";
			}

			if (network.dot11Ssid.uSSIDLength < 2 && network.dot11Ssid.ucSSID[0] == '\0')
			{
				std::cout << "[hidden]";
			}
			else
			{
				std::cout << network.dot11Ssid.ucSSID;
			}

			std::cout << " [length: " << network.dot11Ssid.uSSIDLength << "]" << std::endl;
		}
	}

	WlanFreeMemory(available_networks);
	WlanCloseHandle(handle, nullptr);

	std::cout << std::endl << "Press enter to exit." << std::endl;
	getchar();
	return 0;
}