#include "ProgressBar.hh"
#include <sys/ioctl.h>
#include <math.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace gr
{

ProgressBar::ProgressBar(bool enable_parameter)
{
	this->progressBar_enabled = enable_parameter;
}

ProgressBar::~ProgressBar()
{
}

void ProgressBar::SetShowProgressBar(bool showProgressBar)
{
	this->showProgressBar = showProgressBar;
}

unsigned short int ProgressBar::DetermineTerminalSize()
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	return w.ws_col;
}

std::string ProgressBar::CalculateByteSize(long bytes)
{
	long double KB = bytes / 1024;
	long double MB = KB / 1024;
	long double GB = MB / 1024;
	std::string res;
	std::string suffix;

	std::ostringstream ss;
	ss << std::fixed << std::setprecision(2);

	if (GB > 1)
	{
		ss << GB;
		suffix = "GB";
	}
	else if (MB > 1)
	{
		ss << MB;
		suffix = "MB";
	}
	else
	{
		ss << KB;
		suffix = "KB";
	}

	res = ss.str() + suffix;

	return res;
}

void ProgressBar::PrintProgressBar(long TotalDownloadSize, long finishedDownloadSize, long TotalToUpload, long NowUploaded)
{
	if (this->showProgressBar && this->progressBar_enabled)
	{
		long processed = (TotalDownloadSize > TotalToUpload) ? finishedDownloadSize : NowUploaded;
		long total = (TotalDownloadSize > TotalToUpload) ? TotalDownloadSize : TotalToUpload;

		if (total <= 0.0)
			return;

		//libcurl seems to process more bytes then the actual file size :)
		if (processed > total)
			processed = total;

		int availableSize = this->DetermineTerminalSize() - 32;	//10 for prefix of percent and 22 for suffix of file size

		int totalDots;
		if (availableSize > 100)
			totalDots = 100;
		else if (availableSize < 0)
			totalDots = 10;
		else
			totalDots = availableSize;

		double fraction = (float) processed / total;

		if ((fraction * 100) < 100.0)
			this->hundredpercentDone = false;

		if (!this->hundredpercentDone)
		{
			printf("\33[2K\r");	//delete previous output line

			int dotz = round(fraction * totalDots);
			int count = 0;

			printf("  [%3.0f%%] [", fraction * 100);

			for (; count < dotz - 1; count++)
			{
				printf("=");
			}

			printf(">");

			for (; count < totalDots - 1; count++)
			{
				printf(" ");
			}

			printf("] ");
			printf("%s/%s", this->CalculateByteSize(processed).c_str(), this->CalculateByteSize(total).c_str());
			printf("\r");

			if ((fraction * 100) >= 100.0)
			{
				this->hundredpercentDone = true;
				printf("\n");
			}

			fflush(stdout);
		}

	}

}

}
