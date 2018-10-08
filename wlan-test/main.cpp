#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wlanapi.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono;

static std::atomic<bool> scan_complete = false;

// this callback will be used to wait for WlanScan to complete.
static void wlan_callback(PWLAN_NOTIFICATION_DATA Arg1, PVOID Arg2)
{
	// might be worth handling wlan_notification_acm_scan_fail
	if (Arg1->NotificationCode == wlan_notification_acm_scan_complete)
	{
		scan_complete = true;
	}
}

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

	// this registers a callback that allows us to wait for WlanScan to complete.
	// it will automatically unregister when WlanCloseHandle is called, but can
	// otherwise be unregistered with WLAN_NOTIFICATION_SOURCE_NONE.
	hr = WlanRegisterNotification(handle, WLAN_NOTIFICATION_SOURCE_ACM, true,
	                              &wlan_callback, nullptr, nullptr, 0);

	if (FAILED(hr))
	{
		std::cout << "Warning: WlanRegisterNotification failed. Scan will time out waiting for completion." << std::endl;
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

	std::cout << "Detected interfaces: " << std::endl;

	for (size_t i = 0; i < interface_list->dwNumberOfItems; ++i)
	{
		std::cout << "Interface " << i + 1 << ": ";
		const auto& info = interface_list->InterfaceInfo[i];

		// god damn it microsoft why can't you be normal
		std::wcout << info.strInterfaceDescription << std::endl;

		const GUID& guid = info.InterfaceGuid;

		// this requests a refresh on the list of detected wifi networks
		hr = WlanScan(handle, &guid, nullptr, nullptr, nullptr);

		if (FAILED(hr))
		{
			std::cout << "Warning: WlanScan failed with error code " << hr << std::endl;
		}
		else
		{
			constexpr auto now = []() -> auto
			{
				return high_resolution_clock::now();
			};

			std::cout << "\tWaiting for WlanScan to complete..." << std::endl;

			// microsoft recommends waiting for (and requires that drivers only take)
			// four seconds to complete the scan, so we're gonna bail after that if
			// we don't hear back from the callback.
			const auto start = now();

			while (!scan_complete)
			{
				if (now() - start >= 4s)
				{
					std::cout << "\tWarning: WlanScan took too long to complete." << std::endl;
					break;
				}

				std::this_thread::sleep_for(1ms);
			}

			// this is why printf is better
			std::cout << "\tElapsed time: " << duration_cast<seconds>(now() - start).count() << "s" << std::endl;
		}

		// note that WlanGetAvailableNetworkList2 (*2*) does not exist on Windows 7
		// (not that we needed the info provided by it anyway, but LET IT BE KNOWN)
		PWLAN_AVAILABLE_NETWORK_LIST available_networks = nullptr;
		hr = WlanGetAvailableNetworkList(handle, &guid, 0, nullptr, &available_networks);

		if (FAILED(hr))
		{
			std::cout << "\tWlanGetAvailableNetworkList failed with error code: " << hr << std::endl;
		}
		else
		{
			for (size_t n = 0; n < available_networks->dwNumberOfItems; ++n)
			{
				const auto& network = available_networks->Network[n];

				std::cout << '\t' << n + 1 << ": ";

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
		std::cout << std::endl;
	}

	WlanFreeMemory(interface_list);
	interface_list = nullptr;

	WlanCloseHandle(handle, nullptr);
	std::cout << "Press enter to exit." << std::endl;
	getchar();
	return 0;
}