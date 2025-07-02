#include <syslog.h>

void Info(void)
{
	/*注意这里的数字5与第一条里面提到的local5.*里的5必须相同，并且这个数字的范围为0--7*/
	openlog("info", LOG_PID, LOG_LOCAL5);
	syslog(LOG_INFO, "hello %s", "woring");
	closelog();
}

void Woring(void)
{
	openlog("woring", LOG_PID, LOG_LOCAL5);
	syslog(LOG_WARNING, "hello %s", "test");
	closelog();
}

// journalctl
int main()
{
	Info();
	Woring();
	return 0;
}
