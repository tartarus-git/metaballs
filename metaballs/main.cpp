#define WINDOW_TITLE "Metaballs"
#define WINDOW_CLASS_NAME "METABALLS_WINDOW"
#include "windowSetup.h"

#include "OpenCLBindingsAndHelpers.h"
#include "debugOutput.h"

#define UWM_EXIT_FROM_THREAD WM_USER

LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY: case UWM_EXIT_FROM_THREAD:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int windowWidth;
int windowHeight;
void setWindowSize(unsigned int windowWidth, unsigned int windowHeight) {
	::windowWidth = windowWidth;
	::windowHeight = windowHeight;
}

cl_platform_id computePlatform;
cl_context computeContext;
cl_device_id computeDevice;
cl_command_queue computeCommandQueue;

cl_program computeProgram;
cl_kernel computeKernel;
size_t computeKernelWorkGroupSize;

bool setupComputeDevice() {
	if (!initOpenCLBindings()) {
		debuglogger::out << debuglogger::error << "failed to initialize OpenCL bindings" << debuglogger::endl;
		return true;
	}
	// TODO: Consider making a system where a platform is selected if it's version is at least the version given here through parameters. Would that work?
	cl_int err = initOpenCLVarsForBestDevice("OpenCL 2.1 ", computePlatform, computeDevice, computeContext, computeCommandQueue);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to find the optimal compute device" << debuglogger::endl;
		return true;
	}

	char* buildLog;
	err = setupComputeKernel(computeContext, computeDevice, "metaballRenderer.cl", "metaballRenderer.cl", computeProgram, computeKernel, computeKernelWorkGroupSize, buildLog);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set up compute kernel" << debuglogger::endl;
		if (err == CL_EXT_BUILD_FAILED_WITH_BUILD_LOG) {
			debuglogger::out << debuglogger::error << "failed build" << debuglogger::endl;
			debuglogger::out << "BUILD LOG:" << debuglogger::endl << buildLog << debuglogger::endl;
			delete[] buildLog;																// TODO: Is there any clean way to get rid of the delete[] here?
		}
		return true;
	}
}

void graphicsLoop(HWND hWnd) {
	if (setupComputeDevice()) {
		debuglogger::out << debuglogger::error << "failed to set up compute device" << debuglogger::endl;
		if (!PostMessage(hWnd, UWM_EXIT_FROM_THREAD, 0, 0)) { debuglogger::out << debuglogger::error << "failed to post UWM_EXIT_FROM_THREAD message to window queue" << debuglogger::endl; }
		isAlive = false;
		return;
	}
}