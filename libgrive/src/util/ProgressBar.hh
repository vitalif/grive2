#include <string>

#pragma once

namespace gr
{

class ProgressBar
{

public:
	ProgressBar(bool enable_parameter);
	virtual ~ProgressBar();

	void PrintProgressBar(long TotalDownloadSize, long finishedDownloadSize, long TotalToUpload, long NowUploaded);
	void SetShowProgressBar(bool showProgressBar);

private:
	static std::string CalculateByteSize(long bytes);
	static unsigned short int DetermineTerminalSize();

	bool hundredpercentDone = false;
	bool showProgressBar = false;
	bool progressBar_enabled = false;
};

}
;

