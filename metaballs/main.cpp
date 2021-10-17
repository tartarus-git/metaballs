#define WINDOW_TITLE "Metaballs"
#define WINDOW_CLASS_NAME "METABALLS_WINDOW"
#include "windowSetup.h"
#include "OpenCLBindingsAndHelpers.h"

#include <vector>

#include "debugOutput.h"

#define UWM_EXIT_FROM_THREAD WM_USER

int windowMouseX;
int windowMouseY;
bool mouseUpdated = false;
LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_MOUSEMOVE:
		if (mouseUpdated) { return 0; }
		windowMouseX = LOWORD(lParam);
		windowMouseY = HIWORD(lParam);
		mouseUpdated = true;
		return 0;
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
	err = setupComputeKernel(computeContext, computeDevice, "metaballRenderer.cl", "metaballRenderer", computeProgram, computeKernel, computeKernelWorkGroupSize, buildLog);
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
	cl_int err = clSetKernelArg(computeKernel, 0, sizeof(cl_mem), &positions_computeBuffer);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set positions_computeBuffer kernel arg" << debuglogger::endl;
		return true;
	}

	err = clSetKernelArg(computeKernel, 2, sizeof(cl_mem), &outputFrame_computeImage);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set outputFrame_computeImage kernel arg" << debuglogger::endl;
		return true;
	}

	return false;
}

struct Position {
	int x;
	int y;
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

	positions_computeBuffer = clCreateBuffer(computeContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, positions.size() * sizeof(Position), positions.data(), &err);
	if (!positions_computeBuffer) {
		debuglogger::out << debuglogger::error << "failed to create positions_computeBuffer" << debuglogger::endl;
		return true;
	}

	return setDefaultKernelArgs();
}

bool updateComputePositionsBuffer() {
	cl_int err = clEnqueueWriteBuffer(computeCommandQueue, positions_computeBuffer, true, 0, positions.size() * sizeof(Position), positions.data(), 0, nullptr, nullptr);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to update positions_computeBuffer" << debuglogger::endl;
		return true;
	}
	return false;
}

bool setKernelSizeArgs() {
	int positions_count = positions.size();
	cl_int err = clSetKernelArg(computeKernel, 1, sizeof(int), &positions_count);
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

// TODO: Make this goto the end of the graphicsLoop function where everything gets disposed, so that no memory leaks happen. Figure out the control flow for that in all the areas below.
// Also make it so that the program can return EXIT_FAILURE from thread, and also return EXIT_FAILURE from the window starter function too in case you're not doing that.
#define EXIT_FROM_THREAD if (!PostMessage(hWnd, UWM_EXIT_FROM_THREAD, 0, 0)) { debuglogger::out << debuglogger::error << "failed to post UWM_EXIT_FROM_THREAD message to window queue" << debuglogger::endl; } isAlive = false; return;

void graphicsLoop(HWND hWnd) {
	if (setupComputeDevice()) {
		debuglogger::out << debuglogger::error << "failed to set up compute device" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}

	positions.push_back({ 100, 100 });
	positions.push_back({ 200, 200 });

	if (allocateComputeBuffers()) {
		debuglogger::out << debuglogger::error << "failed to set up compute buffers" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}

	if (setKernelSizeArgs()) {
		debuglogger::out << debuglogger::error << "failed to set kernel size args" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}

	size_t computeGlobalSize[2] = { windowWidth + (computeKernelWorkGroupSize - (windowWidth % computeKernelWorkGroupSize)), windowHeight };
	size_t computeLocalSize[2] = { computeKernelWorkGroupSize, 1 };
	size_t computeFrameOrigin[3] = { 0, 0, 0 };
	size_t computeFrameRegion[3] = { windowWidth, windowHeight, 1 };
	char* outputFrame = new char[windowWidth * windowHeight * 4];

	HDC finalG = GetDC(hWnd);
	HDC g = CreateCompatibleDC(finalG);
	HBITMAP bmp = CreateCompatibleBitmap(finalG, windowWidth, windowHeight);
	SelectObject(g, bmp);

	while (isAlive) {
		if (mouseUpdated) {
			positions[0].x = windowMouseX;
			positions[0].y = windowMouseY;
			if (updateComputePositionsBuffer()) {
				debuglogger::out << debuglogger::error << "failed to move metaball to mouse" << debuglogger::endl;
				EXIT_FROM_THREAD;
			}
			mouseUpdated = false;
		}

		cl_int err = clEnqueueNDRangeKernel(computeCommandQueue, computeKernel, 2, nullptr, computeGlobalSize, computeLocalSize, 0, nullptr, nullptr);
		if (err != CL_SUCCESS) {
			debuglogger::out << debuglogger::error << "failed to enqueue compute kernel" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}

		err = clEnqueueReadImage(computeCommandQueue, outputFrame_computeImage, true, computeFrameOrigin, computeFrameRegion, 0, 0, outputFrame, 0, nullptr, nullptr);
		if (err != CL_SUCCESS) {
			debuglogger::out << debuglogger::error << "failed to read outputFrame_computeImage from compute device" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}
		if (!SetBitmapBits(bmp, windowWidth * windowHeight * 4, outputFrame)) {
			debuglogger::out << debuglogger::error << "failed to set bmp bits" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}
		if (!BitBlt(finalG, 0, 0, windowWidth, windowHeight, g, 0, 0, SRCCOPY)) {
			debuglogger::out << debuglogger::error << "failed to copy g into finalG" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}
	}
	// TODO: Manage exiting using isAlive = false from window thread. BitBlt fails when window is closing, which isn't good at all. Devise a way to exit from the message loop instead of quitting that first.
	delete[] outputFrame;
	if (!ReleaseDC(hWnd, finalG)) { debuglogger::out << debuglogger::error << "failed to release window DC (finalG)" << debuglogger::endl; }
	if (!DeleteDC(g)) { debuglogger::out << debuglogger::error << "failed to delete memory DC (g)" << debuglogger::endl; }
	if (!DeleteObject(bmp)) { debuglogger::out << debuglogger::error << "failed to delete bmp" << debuglogger::endl; }							// This needs to be deleted after it is no longer selected by any DC.
}