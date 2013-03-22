// drfifoutil.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>
#include <string>

#include "tstuff.h"
#include "../driver/drfifo_ioctl.h"

using namespace std;

#define PROGRAM_NAME  "drfifoutil"

#ifdef UNICODE
#define T_PROGRAM_NAME  L"drfifoutil"
#else
#define T_PROGRAM_NAME  PROGRAM_NAME
#endif

// ----------------------------------------------------------------------------
/**
 * Prints usage info to stderr.
 */
void usage(void)
{
	tcerr << endl;
	tcerr << M_T("Usage: ") << T_PROGRAM_NAME << M_T(" <device-service-name> <command> [args...]") << endl;
	tcerr << endl;
	tcerr << M_T("Commands are 'status', 'read', 'write', 'reset' and 'flush'.") << endl;
	tcerr << endl;
}   // usage()

// ----------------------------------------------------------------------------
/**
 * @param error - a value returned by GetLastError().
 *
 * @return a string representation of the error.
 */
tstring error_message(DWORD error_number)
{
	tstring result = M_T("uknown");
	LPTSTR heap_message = NULL;
	DWORD format_result = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
										  FORMAT_MESSAGE_FROM_SYSTEM |
										  FORMAT_MESSAGE_ARGUMENT_ARRAY,
										  0, error_number, LANG_NEUTRAL, (LPTSTR) &heap_message, 0, 0);

	if (0 != format_result)
	{
		result = heap_message;
	}

	if (heap_message)
	{
		LocalFree(HLOCAL(heap_message));
	}

	if (!result.empty() && (result.at(result.size()-1) == '\n'))
	{
		result.erase(result.size()-1, 1);
	}

	return result;
}   // error_message()

// ----------------------------------------------------------------------------
/**
 * Handles a status command by issuing a DRFIFO_IOCTL_STATUS device control.
 *
 * @param device - file handle for the open device.
 */
void handle_status(HANDLE device)
{
	drfifo_ioctl_status_t status;
	memset(&status, 0, sizeof(status));

	DWORD bytes_read = 0;
	BOOL result = DeviceIoControl(device,
								  DRFIFO_IOCTL_STATUS,      // IOCTL command.
								  NULL, 0,                  // Input buffer (info going into the device).
								  &status, sizeof(status),  // Output buffer (info coming out of the device).
								  &bytes_read,              // Bytes read. Should be zero. (???)
								  NULL);                    // For OVERLAPPED (we're not).
	if (!result)
	{
		DWORD error = ::GetLastError();
		tcerr << T_PROGRAM_NAME << M_T(": DeviceIoControl() failed with error ") << error
			  << M_T(": ") << error_message(error) << endl;
	}
	else
	{
		tcout << M_T("size      = ") << status.size      << endl;
		tcout << M_T("flags     = ") << status.flags     << endl;
		tcout << M_T("put_count = ") << status.put_count << endl;
		tcout << M_T("get_count = ") << status.get_count << endl;
		const size_t bytes_in_fifo = status.put_count - status.get_count;
		tcout << M_T("bytes available for put = ") << (status.size - bytes_in_fifo) << endl;
		tcout << M_T("bytes available for get = ") << bytes_in_fifo << endl;
	}
}   // handle_status()

// ----------------------------------------------------------------------------
/**
 * Handles a write command by calling WriteFile().
 *
 * @param device - file handle for the open device.
 */
void handle_write(HANDLE device, int num_args, _TCHAR* arg[])
{
	const uint8_t data[0x40] = "test_string.";
	DWORD written = 0;
	BOOL result = WriteFile(device, data, (DWORD) strlen((const char*) data)+1, &written, 0);

	if (!result)
	{
		DWORD error = ::GetLastError();
		tcerr << T_PROGRAM_NAME << M_T(": WriteFile() failed with error ") << error
			  << M_T(": ") << error_message(error) << endl;
	}
	else
	{
		tcout << M_T("wrote ") << written << M_T(" bytes to device.") << endl;
	}
}   // handle_write()

// ----------------------------------------------------------------------------
/**
 * Handles a read command by calling ReadFile().
 *
 * @param device - file handle for the open device.
 */
void handle_read(HANDLE device, int num_args, _TCHAR* arg[])
{
	uint8_t data[0x40] = "";
	DWORD bytes_read = 0;
	BOOL result = ReadFile(device, data, sizeof(data), &bytes_read, 0);

	if (!result)
	{
		DWORD error = ::GetLastError();
		tcerr << T_PROGRAM_NAME << M_T(": ReadFile() failed with error ") << error
			  << M_T(": ") << error_message(error) << endl;
	}
	else
	{
		tcout << M_T("read ") << bytes_read << M_T(" bytes from device.") << endl;
		 cout << "\"" << data << "\"" << endl;
	}
}   // handle_write()

// ----------------------------------------------------------------------------
/**
 * Main program.
 */
int _tmain(int argc, _TCHAR* argv[])
{
	cout << PROGRAM_NAME << ": " << __DATE__ << " " << __TIME__ << "." << endl;

	if (argc < 3)
	{
		usage();
		return 1;
	}

	tstring device_name(argv[1]);
	tstring command(argv[2]);

	// tstring device_path = M_T("\\\\.\\Global\\");		// Pre-Vista?: M_T("\\\\.\\");
	tstring device_path = M_T("\\\\.\\");

	if (device_name.at(0) == '\\')
	{
		device_path = device_name;
	}
	else
	{
		device_path += device_name;
	}

	tcout << T_PROGRAM_NAME << M_T(": calling CreateFile(\"") << device_path << M_T("\").") << endl;

	HANDLE device = CreateFile(device_path.c_str(),
							   GENERIC_READ | GENERIC_WRITE,
							   0,  // FILE_SHARE_READ | FILE_SHARE_WRITE
							   NULL,
							   OPEN_EXISTING,
							   0,  // FILE_ATTRIBUTE_NORMAL,
							   NULL);

	if (INVALID_HANDLE_VALUE == device)
	{
		NTSTATUS error = ::GetLastError();
		tcerr << T_PROGRAM_NAME << M_T(": CreateFile(\"") << device_path << M_T("\") failed ")
			  << error << M_T(": ") << error_message(error);
		return 2;
	}

	int result = 0;

	if (command == M_T("status"))		handle_status(device);
	else if (command == M_T("write"))	handle_write(device, argc - 3, &argv[3]);
	else if (command == M_T("read"))	handle_read(device, argc - 3, &argv[3]);
	else
	{
		tcerr << T_PROGRAM_NAME << ": unsupported command \"" << command << "\"." << endl;
		result = 2;
	}

	CloseHandle(device);
	return 0;
}   // _tmain()
