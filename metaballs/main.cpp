#define WINDOW_TITLE "Metaballs"
#define WINDOW_CLASS_NAME "METABALLS_WINDOW"
#include "windowSetup.h"
#include "OpenCLBindingsAndHelpers.h"

#include <vector>

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

cl_mem outputFrame_computeImage;
cl_mem positions_computeBuffer;

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

bool setDefaultKernelArgs() {
	cl_int err = clSetKernelArg(computeKernel, 0, sizeof(cl_mem), positions_computeBuffer);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set positions_computeBuffer kernel arg" << debuglogger::endl;
		return true;
	}

	err = clSetKernelArg(computeKernel, 2, sizeof(cl_mem), outputFrame_computeImage);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set outputFrame_computeImage kernel arg" << debuglogger::endl;
		return true;
	}

	return false;
}

struct Position {
	float x;
	float y;
};

const cl_image_format computeImageFormat = { CL_RGBA, CL_UNSIGNED_INT8 };
std::vector<Position> positions;
bool allocateComputeBuffers() {
	cl_int err;
	outputFrame_computeImage = clCreateImage2D(computeContext, CL_MEM_WRITE_ONLY, &computeImageFormat, windowWidth, windowHeight, 0, nullptr, &err);
	if (!outputFrame_computeImage) {
		debuglogger::out << debuglogger::error << "failed to create outputFrame_computeImage" << debuglogger::endl;
		return true;
	}

	positions_computeBuffer = clCreateBuffer(computeContext, CL_MEM_READ_ONLY, positions.size() * sizeof(float), positions.data(), &err);
	if (!positions_computeBuffer) {
		debuglogger::out << debuglogger::error << "failed to create positions_computeBuffer" << debuglogger::endl;
		return true;
	}

	return setDefaultKernelArgs();
}

bool setKernelSizeArgs() {
	size_t positions_count = positions.size();
	cl_int err = clSetKernelArg(computeKernel, 1, sizeof(unsigned int), &positions_count);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set positions_count kernel arg" << debuglogger::endl;
		return true;
	}

	err = clSetKernelArg(computeKernel, 3, sizeof(unsigned int), &windowWidth);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set windowWidth kernel arg" << debuglogger::endl;
		return true;
	}
	err = clSetKernelArg(computeKernel, 4, sizeof(unsigned int), &windowHeight);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set windowHeight kernel arg" << debuglogger::endl;
		return true;
	}
}

#define EXIT_FROM_THREAD if (!PostMessage(hWnd, UWM_EXIT_FROM_THREAD, 0, 0)) { debuglogger::out << debuglogger::error << "failed to post UWM_EXIT_FROM_THREAD message to window queue" << debuglogger::endl; } isAlive = false; return;

void graphicsLoop(HWND hWnd) {
	if (setupComputeDevice()) {
		debuglogger::out << debuglogger::error << "failed to set up compute device" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}

	if (allocateComputeBuffers()) {
		debuglogger::out << debuglogger::error << "failed to set up compute buffers" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}

	positions.push_back({ 100, 100 });
	positions.push_back({ 200, 200 });

	if (setKernelSizeArgs()) {
		debuglogger::out << debuglogger::error << "failed to set kernel size args" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}
}