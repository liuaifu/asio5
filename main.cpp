#if !defined (_WIN32) && !defined (_WIN64)
#include <unistd.h>
#else
#include "targetver.h"
#endif

#include <cstdlib>
#include <iostream>
#include <set>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include "service.h"

using namespace boost;
using boost::asio::ip::tcp;

boost::asio::io_service *g_io_service = nullptr;

#ifdef _WIN32
std::string GetLastErrorMsg(DWORD dwError = 0xFFFFFFFF)
{
	if (dwError == 0xFFFFFFFF)
		dwError = GetLastError();
	LPVOID lpBuffer = NULL;
	DWORD dwResult = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwError,
		0,
		(LPTSTR)&lpBuffer,
		0,
		NULL
	);
	
	std::string msg;
	if (dwResult) {
		msg = (LPTSTR)lpBuffer;
		LocalFree(lpBuffer);
	}

	return msg;
}

BOOL EnableDebugPriv()
{
	HANDLE hToken;
	LUID sedebugnameValue;
	TOKEN_PRIVILEGES tkp;

	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return false;

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue))
	{
		CloseHandle(hToken);
		return false;
	}

	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Luid = sedebugnameValue;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof tkp, NULL, NULL))
		CloseHandle(hToken);

	return true;
}

static char g_szServiceName[64] = "asio5";

void ControlHandler(DWORD);
static SERVICE_STATUS ServiceStatus;
static SERVICE_STATUS_HANDLE hStatus;

void WINAPI ServiceMain(int argc, char** argv)
{
	ServiceStatus.dwServiceType = SERVICE_WIN32;
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;
	hStatus = RegisterServiceCtrlHandler(g_szServiceName, (LPHANDLER_FUNCTION)ControlHandler);
	if (hStatus == (SERVICE_STATUS_HANDLE)0)
	{
		// Registering Control Handler failed
		OutputDebugString("Registering Control Handler failed");
		return;
	}
	//Report the running status to SCM. 
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hStatus, &ServiceStatus);

	return;
}

void ControlHandler(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &ServiceStatus);

		g_io_service->stop();

		return;

	case SERVICE_CONTROL_SHUTDOWN:
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &ServiceStatus);

		g_io_service->stop();

		return;

	default:
		break;
	}
	// Report current status
	SetServiceStatus(hStatus, &ServiceStatus);
}

//判断服务是否已经被安装
BOOL IsInstalled()
{
	BOOL bResult = FALSE;

	//打开服务控制管理器  
	SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, GENERIC_READ);
	if (hSCM == NULL) {
		MessageBox(0, GetLastErrorMsg().c_str(), "错误", MB_OK|MB_ICONERROR);
		std::exit(1);
	}

	//打开服务  
	SC_HANDLE hService = ::OpenService(hSCM, g_szServiceName, SERVICE_QUERY_CONFIG);
	if (hService != NULL) {
		bResult = TRUE;
		::CloseServiceHandle(hService);
	}
	else {
		DWORD dwError = GetLastError();
		if (dwError != ERROR_SERVICE_DOES_NOT_EXIST) {
			MessageBox(0, GetLastErrorMsg(dwError).c_str(), "错误", MB_OK | MB_ICONERROR);
			::CloseServiceHandle(hSCM);
			std::exit(1);
		}
	}
	::CloseServiceHandle(hSCM);

	return bResult;
}

//安装服务函数
void Install()
{
	//检测是否安装过
	if (IsInstalled()) {
		MessageBox(0, "已经安装过了！", "警告", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	//打开服务控制管理器
	SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCM == NULL)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_ACCESS_DENIED)
			MessageBox(0, "权限不足！需要以管理员权限运行。", "错误", MB_OK | MB_ICONERROR);
		else
			MessageBox(0, GetLastErrorMsg(dwError).c_str(), "错误", MB_OK | MB_ICONERROR);
		std::exit(1);
	}

	//获取程序目录
	TCHAR szFilePath[MAX_PATH];
	::GetModuleFileName(NULL, szFilePath, MAX_PATH);
	strcat(szFilePath, " --service");

	//创建服务  
	SC_HANDLE hService = ::CreateService(hSCM, g_szServiceName, g_szServiceName,
		SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
		szFilePath, NULL, NULL, "", NULL, NULL);

	//检测创建是否成功
	if (hService == NULL)
	{
		::CloseServiceHandle(hSCM);
		MessageBox(0, GetLastErrorMsg().c_str(), "错误", MB_OK | MB_ICONERROR);
		std::exit(1);
	}

	//释放资源
	::CloseServiceHandle(hService);
	::CloseServiceHandle(hSCM);

	MessageBox(0, "注册服务成功。", "提示", MB_OK | MB_ICONINFORMATION);
}
#endif

void init_log(boost::log::trivial::severity_level log_level)
{
	//boost::log::add_console_log(std::cout, boost::log::keywords::format = (
	//	boost::log::expressions::stream
	//	<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%H:%M:%S.%f")
	//	<< " <" << boost::log::trivial::severity
	//	<< "> " << boost::log::expressions::smessage)
	//);
	if ((int)log_level == -1) {
		boost::log::core::get()->remove_all_sinks();
		return;	//-1 no log
	}

	boost::log::add_file_log
	(
		boost::log::keywords::file_name = "log/asio5_%Y%m%d_%N.log",
		boost::log::keywords::rotation_size = 50 * 1024 * 1024,
		boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
		boost::log::keywords::auto_flush = true,
		boost::log::keywords::open_mode = std::ios::app,
		boost::log::keywords::format = (
			boost::log::expressions::stream
			<< boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
			<< "|" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
			<< "|" << boost::log::trivial::severity
			<< "|" << boost::log::expressions::smessage
			)
	);
	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= log_level
	);
	boost::log::add_common_attributes();
}

unsigned int get_core_count()
{
	unsigned count = 1;
#if !defined (_WIN32) && !defined (_WIN64)
	count = sysconf(_SC_NPROCESSORS_CONF);	//_SC_NPROCESSORS_ONLN
#else
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	count = si.dwNumberOfProcessors;
#endif  
	return count;
}

int main(int argc, char* argv[])
{
	using namespace boost::program_options;
	//声明需要的选项
	options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
#ifdef _WIN32
		("register", "register as a win32 service")
		("service", "run as a win32 service")
#endif
		("port", value<int>()->default_value(7070), "listen port")
		("log-level", value<int>()->default_value(1), "-1:no log,0:trace,1:debug,2:info,3:warning,4:error,5:fatal")
		;

	variables_map vm;
	store(parse_command_line(argc, argv, desc), vm);
	notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	if (vm.count("register") > 0) {
		Install();
		return 0;
	}
	bool win32_service = vm.count("service") > 0;

	int log_level = vm["log-level"].as<int>();
	if (log_level < -1 || log_level > 5) {
		std::cerr << "log-level invalid" << std::endl;
		std::exit(-1);
	}

	init_log((boost::log::trivial::severity_level)log_level);

	BOOST_LOG_TRIVIAL(info) << "--------------------------------------------------";
	BOOST_LOG_TRIVIAL(info) << "asio5 v0.6";
	BOOST_LOG_TRIVIAL(info) << "laf163@gmail.com";
	BOOST_LOG_TRIVIAL(info) << "compiled at " << __DATE__ << " " << __TIME__;

	std::vector<boost::shared_ptr<boost::thread> > threads;

	int port = vm["port"].as<int>();
	boost::asio::io_service io_service_;
	g_io_service = &io_service_;
	boost::shared_ptr<service> service_ptr(new service(io_service_, port));
	service_ptr->start();
	BOOST_LOG_TRIVIAL(info) << "work thread count: " << get_core_count();
	BOOST_LOG_TRIVIAL(info) << "listening on 0.0.0.0:" << port;
	for (unsigned int i = 0; i < get_core_count(); i++) {
		threads.push_back(
			boost::shared_ptr<boost::thread>(
				new boost::thread(
					boost::bind(&asio::io_service::run, &io_service_)
				)
			)
		);
	}

#ifdef _WIN32
	if (win32_service) {
		SERVICE_TABLE_ENTRY ServiceTable[2];
		ServiceTable[0].lpServiceName = g_szServiceName;
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;
		StartServiceCtrlDispatcher(ServiceTable);
	}
#endif

	printf("running...");
	for(auto& t: threads)
		t->join();

	return 0;
}
